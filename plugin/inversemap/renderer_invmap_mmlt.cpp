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
#include "multiplexeddensity.h"

LM_NAMESPACE_BEGIN

///! Multiplexed metropolis light transport
class Renderer_Invmap_MMLT final : public Renderer
{
public:

    LM_IMPL_CLASS(Renderer_Invmap_MMLT, Renderer);

public:

    int maxNumVertices_;
    long long numMutations_;
    double renderTime_;
    long long numSeedSamples_;
    double seedRenderTime_;
    Float largeStepProb_;

public:

    LM_IMPL_F(Initialize) = [this](const PropertyNode* prop) -> bool
    {
        if (!prop->ChildAs<int>("max_num_vertices", maxNumVertices_)) return false;
        numMutations_ = prop->ChildAs<long long>("num_mutations", 0);
        renderTime_ = prop->ChildAs<double>("render_time", -1);
        numSeedSamples_ = prop->ChildAs<long long>("num_seed_samples", 0);
        seedRenderTime_ = prop->ChildAs<double>("seed_render_time", -1);
        largeStepProb_ = prop->ChildAs<Float>("large_step_prob", 0.5_f);
        return true;
    };

    LM_IMPL_F(Render) = [this](const Scene* scene, Random* initRng, const std::string& outputPath) -> void
    {
        auto* film = static_cast<const Sensor*>(scene->GetSensor()->emitter)->GetFilm();

        // --------------------------------------------------------------------------------

        #pragma region Sample candidates for seed paths and normalization factor estimation
        const auto b = [&]() -> std::vector<Float>
        {
            LM_LOG_INFO("Computing normalizagion factor");
            LM_LOG_INDENTER();

            struct Context
            {
                Random rng;
                std::vector<Float> b;
            };
            std::vector<Context> contexts(Parallel::GetNumThreads());
            for (auto& ctx : contexts)
            {
                ctx.rng.SetSeed(initRng->NextUInt());
                ctx.b.assign(maxNumVertices_ - 1, 0_f);
            }

            const auto processed = Parallel::For({ seedRenderTime_ < 0 ? ParallelMode::Samples : ParallelMode::Time, numSeedSamples_, seedRenderTime_ }, [&](long long index, int threadid, bool init)
            {
                auto& ctx = contexts[threadid];

                Subpath subpathE;
                Subpath subpathL;
                subpathE.SampleSubpathFromEndpoint(scene, &ctx.rng, TransportDirection::EL, maxNumVertices_);
                subpathL.SampleSubpathFromEndpoint(scene, &ctx.rng, TransportDirection::LE, maxNumVertices_);

                const int nL = (int)(subpathL.vertices.size());
                const int nE = (int)(subpathE.vertices.size());
                for (int n = 2; n <= nE + nL; n++)
                {
                    if (n > maxNumVertices_) { continue; }
                    const int minS = Math::Max(0, n - nE);
                    const int maxS = Math::Min(nL, n);
                    for (int s = minS; s <= maxS; s++)
                    {
                        const int t = n - s;

                        Path fullpath;
                        if (!fullpath.ConnectSubpaths(scene, subpathL, subpathE, s, t)) { continue; }

                        const auto Cstar = fullpath.EvaluateUnweightContribution(scene, s);
                        if (Cstar.Black()) { continue; }

                        const auto w = fullpath.EvaluateMISWeight(scene, s);
                        const auto C = Cstar * w;
                        
                        ctx.b[n - 2] += InversemapUtils::ScalarContrb(C);
                    }
                }
            });

            std::vector<Float> b(maxNumVertices_ - 1);
            for (auto& ctx : contexts) { std::transform(b.begin(), b.end(), ctx.b.begin(), b.begin(), std::plus<Float>()); }
            for (auto& v : b) { v /= (Float)(processed); }

            {
                LM_LOG_INFO("Normalization factor(s)");
                LM_LOG_INDENTER();
                for (size_t k = 0; k < b.size(); k++)
                {
                    LM_LOG_INFO(boost::str(boost::format("k = %d: %.10f") % k % b[k]));
                }
            }
            
            return b;
        }();
        #pragma endregion

        // --------------------------------------------------------------------------------

        #pragma region Construct PMF for path length sampling
        Distribution1D pathLengthDist;
        {
            pathLengthDist.Clear();
            for (auto& v : b) { pathLengthDist.Add(v); }
            pathLengthDist.Normalize();
        }
        #pragma endregion

        // --------------------------------------------------------------------------------

        #pragma region Rendering
        {
            LM_LOG_INFO("Rendering");
            LM_LOG_INDENTER();

            struct Context
            {
                Random rng;
                Film::UniquePtr film{ nullptr, nullptr };
                std::vector<MultiplexedDensity::State> curr;
            };
            std::vector<Context> contexts(Parallel::GetNumThreads());
            for (auto& ctx : contexts)
            {
                ctx.rng.SetSeed(initRng->NextUInt());
                ctx.film = ComponentFactory::Clone<Film>(film);
                ctx.curr.assign(maxNumVertices_-1, MultiplexedDensity::State());

                // Initial state
                for (int k = 0; k < maxNumVertices_ - 1; k++)
                {
                    //LM_LOG_INFO("Setting initial state for k = " + std::to_string(k));
                    //LM_LOG_INDENTER();

                    // Skip if no valid path with given length
                    if (pathLengthDist.EvaluatePDF(k) < Math::EpsLarge())
                    {
                        //LM_LOG_INFO("Skipping");
                        continue;
                    }

                    // Find initial state for given length
                    long long i;
                    const long long MaxInitialStateIter = 10000000;
                    for (i = 0; i < MaxInitialStateIter; i++)
                    {
                        MultiplexedDensity::State state(initRng, k+2);
                        const auto path = MultiplexedDensity::InvCDF(state, scene);
                        if (!path)
                        {
                            continue;
                        }

                        //LM_LOG_INFO("Found with iter = " + std::to_string(i));
                        ctx.curr[k] = std::move(state);
                        break;
                    }
                    if (i == MaxInitialStateIter)
                    {
                        //LM_LOG_INFO("Skipping");
                        continue;
                    }
                }
            }

            // --------------------------------------------------------------------------------

            const auto processed = Parallel::For({ renderTime_ < 0 ? ParallelMode::Samples : ParallelMode::Time, numMutations_, renderTime_ }, [&](long long index, int threadid, bool init) -> void
            {
                auto& ctx = contexts[threadid];

                // --------------------------------------------------------------------------------
                
                #pragma region Select a path length
                const auto k = pathLengthDist.Sample(ctx.rng.Next());
                if (pathLengthDist.EvaluatePDF(k) < Math::EpsLarge())
                {
                    return;
                }
                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Mutation
                const auto accept = [&]() -> bool
                {
                    // Mutate
                    auto prop = ctx.rng.Next() < largeStepProb_
                        ? ctx.curr[k].LargeStep(&ctx.rng)
                        : ctx.curr[k].SmallStep(&ctx.rng);

                    // Paths
                    const auto currP = MultiplexedDensity::InvCDF(ctx.curr[k], scene);
                    const auto propP = MultiplexedDensity::InvCDF(prop, scene);
                    if (!propP)
                    {
                        return false;
                    }

                    // Scalar contributions
                    const auto currC = InversemapUtils::ScalarContrb(currP->Cstar * currP->w);
                    const auto propC = InversemapUtils::ScalarContrb(propP->Cstar * propP->w);

                    // MH update
                    const auto A = currC == 0_f ? 1_f : Math::Min(1_f, propC / currC);
                    if (ctx.rng.Next() < A)
                    {
                        ctx.curr[k].Swap(prop);
                        return true;
                    }

                    return false;
                }();
                LM_UNUSED(accept);
                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Accumulate contribution
                {
                    const auto p = MultiplexedDensity::InvCDF(ctx.curr[k], scene);
                    assert(p);
                    const auto C = p->Cstar * p->w;
                    const auto I = InversemapUtils::ScalarContrb(C);
                    ctx.film->Splat(p->path.RasterPosition(), C * (b[k] / I) / pathLengthDist.EvaluatePDF(k));
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
            film->Rescale((Float)(film->Width() * film->Height()) / processed);
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

LM_COMPONENT_REGISTER_IMPL(Renderer_Invmap_MMLT, "renderer::invmap_mmlt");

LM_NAMESPACE_END
