/*
    Lightmetrica - A modern, research-oriented renderer

    Copyright (c) 2015 Hisanari Otsu

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.
*/

#include "inversemaputils.h"
#include <boost/format.hpp>

LM_NAMESPACE_BEGIN

///! Primary sample space metropolis light transport (fixed path length)
class Renderer_Invmap_PSSMLTFixed final : public Renderer
{
public:

    LM_IMPL_CLASS(Renderer_Invmap_PSSMLTFixed, Renderer);

public:

    int numVertices_;
    long long numMutations_;
    long long numSeedSamples_;
    Float largeStepProb_;
    #if INVERSEMAP_OMIT_NORMALIZATION
    Float normalization_;
    #endif
    std::string pathType_;

public:

    LM_IMPL_F(Initialize) = [this](const PropertyNode* prop) -> bool
    {
        if (!prop->ChildAs<int>("num_vertices", numVertices_)) return false;
        if (!prop->ChildAs<long long>("num_mutations", numMutations_)) return false;
        if (!prop->ChildAs<long long>("num_seed_samples", numSeedSamples_)) return false;
        largeStepProb_ = prop->ChildAs<Float>("large_step_prob", 0.5_f);
        #if INVERSEMAP_OMIT_NORMALIZATION
        normalization_ = prop->ChildAs<Float>("normalization", 1_f);
        #endif
        pathType_ = prop->ChildAs<std::string>("path_type", "");
        return true;
    };

    LM_IMPL_F(Render) = [this](const Scene* scene, Random* initRng, const std::string& outputPath) -> void
    {
        auto* film = static_cast<const Sensor*>(scene->GetSensor()->emitter)->GetFilm();

        // --------------------------------------------------------------------------------

        #pragma region Compute normalization factor
        #if INVERSEMAP_OMIT_NORMALIZATION
        const auto b = normalization_;
        #else
        const auto b = [&]() -> Float
        {
            LM_LOG_INFO("Computing normalization factor");
            LM_LOG_INDENTER();
            
            struct Context
            {
                Random rng;
                Float b = 0_f;
            };
            std::vector<Context> contexts(Parallel::GetNumThreads());
            for (auto& ctx : contexts) { ctx.rng.SetSeed(initRng->NextUInt()); }
            
            Parallel::For(numSeedSamples_, [&](long long index, int threadid, bool init)
            {
                auto& ctx = contexts[threadid];
                
                // Generate primary sample
                std::vector<Float> ps;
                for (int i = 0; i < InversemapUtils::NumSamples(numVertices_); i++)
                {
                    ps.push_back(ctx.rng.Next());
                }

                // Map to path
                const auto p = InversemapUtils::MapPS2Path(scene, ps);
                if (!p || (int)p->vertices.size() != numVertices_)
                {
                    return;
                }

                // Accumulate contribution
                ctx.b += InversemapUtils::ScalarContrb(p->EvaluateF(0) / p->EvaluatePathPDF(scene, 0));
            });

            Float b = 0_f;
            for (auto& ctx : contexts) { b += ctx.b; }
            b /= numSeedSamples_;

            LM_LOG_INFO(boost::str(boost::format("Normalization factor: %.10f") % b));
            return b;
        }();
        #endif
        #pragma endregion

        // --------------------------------------------------------------------------------

        #pragma region Rendering
        {
            LM_LOG_INFO("Rendering");
            LM_LOG_INDENTER();

            // --------------------------------------------------------------------------------

            // Thread-specific context
            struct Context
            {
                Random rng;
                Film::UniquePtr film{nullptr, nullptr};
                std::vector<Float> currPS;
            };
            std::vector<Context> contexts(Parallel::GetNumThreads());
            for (auto& ctx : contexts)
            {
                ctx.rng.SetSeed(initRng->NextUInt());
                ctx.film = ComponentFactory::Clone<Film>(film);

                // Initial state
                while (true)
                {
                    // Generate initial sample with positive contribution with path tracing
                    // Ignore start-up bias here
                    std::vector<Float> ps;
                    for (int i = 0; i < InversemapUtils::NumSamples(numVertices_); i++)
                    {
                        ps.push_back(initRng->Next());
                    }

                    const auto path = InversemapUtils::MapPS2Path(scene, ps);
                    if (!path || (int)path->vertices.size() != numVertices_ || path->EvaluateF(0).Black())
                    {
                        continue;
                    }

                    ctx.currPS = ps;
                    break;
                }
            }

            // --------------------------------------------------------------------------------

            Parallel::For(numMutations_, [&](long long index, int threadid, bool init) -> void
            {
                auto& ctx = contexts[threadid];

                // --------------------------------------------------------------------------------

                #pragma region Small step mutation in primary sample space
                [&]() -> void
                {
                    #pragma region Mutate

                    const auto LargeStep = [this](const std::vector<Float>& currPS, Random& rng) -> std::vector <Float>
                    {
                        assert(currPS.size() == numVertices_);
                        std::vector<Float> propPS;
                        for (int i = 0; i < InversemapUtils::NumSamples(numVertices_); i++)
                        {
                            propPS.push_back(rng.Next());
                        }
                        return propPS;
                    };

                    const auto SmallStep = [this](const std::vector<Float>& ps, Random& rng) -> std::vector<Float>
                    {
                        const auto Perturb = [](Random& rng, const Float u, const Float s1, const Float s2)
                        {
                            Float result;
                            Float r = rng.Next();
                            if (r < 0.5_f)
                            {
                                r = r * 2_f;
                                result = u + s2 * std::exp(-std::log(s2 / s1) * r);
                                if (result > 1_f) result -= 1_f;
                            }
                            else
                            {
                                r = (r - 0.5_f) * 2_f;
                                result = u - s2 * std::exp(-std::log(s2 / s1) * r);
                                if (result < 0_f) result += 1_f;
                            }
                            return result;
                        };

                        std::vector<Float> propPS;
                        for (const Float u : ps)
                        {
                            //propPS.push_back(Perturb(rng, u, 1_f / 64_f, 1_f / 4_f));
                            //propPS.push_back(Perturb(rng, u, 1_f / 1024_f, 1_f / 64_f));
                            propPS.push_back(Perturb(rng, u, 1_f / 256_f, 1_f / 16_f));
                        }

                        return propPS;
                    };

                    auto propPS = ctx.rng.Next() < largeStepProb_
                        ? LargeStep(ctx.currPS, ctx.rng)
                        : SmallStep(ctx.currPS, ctx.rng);

                    #pragma endregion

                    // --------------------------------------------------------------------------------

                    #pragma region MH update

                    // Function to compute path contribution
                    const auto PathContrb = [&](const Path& path) -> SPD
                    {
                        const auto F = path.EvaluateF(0);
                        assert(!F.Black());
                        SPD C;
                        if (!F.Black())
                        {
                            const auto P = path.EvaluatePathPDF(scene, 0);
                            assert(P > 0);
                            C = F / P;
                        }
                        return C;
                    };

                    // Map primary samples to paths
                    const auto currP = InversemapUtils::MapPS2Path(scene, ctx.currPS);
                    const auto propP = InversemapUtils::MapPS2Path(scene, propPS);

                    // Immediately rejects if the proposed path is invalid or the dimension changes
                    if (!propP || currP->vertices.size() != propP->vertices.size())
                    {
                        return;
                    }

                    // Evaluate contributions
                    const Float currC = InversemapUtils::ScalarContrb(PathContrb(*currP));
                    const Float propC = InversemapUtils::ScalarContrb(PathContrb(*propP));

                    // Acceptance ratio
                    const Float A = currC == 0 ? 1 : Math::Min(1_f, propC / currC);

                    // Accept or reject?
                    if (ctx.rng.Next() < A)
                    {
                        ctx.currPS.swap(propPS);
                    }

                    #pragma endregion
                }();
                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Accumulate contribution
                {
                    auto currP = InversemapUtils::MapPS2Path(scene, ctx.currPS);
                    const auto currF = currP->EvaluateF(0);
                    if (!currF.Black() && currP->IsPathType(pathType_))
                    {
                        ctx.film->Splat(currP->RasterPosition(), currF * (b / InversemapUtils::ScalarContrb(currF)));
                        //ctx.film->Splat(currP->RasterPosition(), SPD(b));
                    }
                }
                #pragma endregion
            });

            // --------------------------------------------------------------------------------

            // Gather & Rescale
            film->Clear();
            for (auto& ctx : contexts)
            {
                film->Accumulate(ctx.film.get());
            }
            film->Rescale((Float)(film->Width() * film->Height()) / numMutations_);
        }
        #pragma endregion

        // --------------------------------------------------------------------------------

        #pragma region Save image
        {
            LM_LOG_INFO("Saving image");
            LM_LOG_INDENTER();
            film->Save(outputPath);
        }
        #pragma endregion
    };

};

LM_COMPONENT_REGISTER_IMPL(Renderer_Invmap_PSSMLTFixed, "renderer::invmap_pssmltfixed");

LM_NAMESPACE_END
