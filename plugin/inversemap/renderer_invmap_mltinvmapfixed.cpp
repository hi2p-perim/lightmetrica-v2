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

#include "mltutils.h"

#define INVERSEMAP_MLTINVMAPFIXED_DEBUG_OUTPUT_TRIANGLE 0
#define INVERSEMAP_MLTINVMAPFIXED_DEBUG_TRACEPLOT 0
#define INVERSEMAP_MLTINVMAPFIXED_DEBUG_LONGEST_REJECTION 0
#define INVERSEMAP_MLTINVMAPFIXED_DEBUG_COUNT_OCCURRENCES 0

LM_NAMESPACE_BEGIN

enum class InvmapStrategy : int
{
    // Path space mutations
    Bidir       = (int)(Strategy::Bidir),
    Lens        = (int)(Strategy::Lens),
    Caustic     = (int)(Strategy::Caustic),
    Multichain  = (int)(Strategy::Multichain),
    Identity    = (int)(Strategy::Identity),
        
    // Primary sample space mutations
    SmallStep,
    LargeStep,
};

///! Combining PSSMLT and MLT via inversemap (fixed path length)
class Renderer_Invmap_MLTInvmapFixed final : public Renderer
{
public:

    LM_IMPL_CLASS(Renderer_Invmap_MLTInvmapFixed, Renderer);

public:

    int numVertices_;
    long long numMutations_;
    long long numSeedSamples_;
    std::vector<Float> strategyWeights_{ 1_f, 1_f, 1_f, 1_f, 1_f, 1_f, 1_f };
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

        {
            LM_LOG_INFO("Loading mutation strategy weights");
            LM_LOG_INDENTER();
            const auto* child = prop->Child("mutation_strategy_weights");
            if (!child)
            {
                LM_LOG_ERROR("Missing 'mutation_strategy_weights'");
                return false;
            }
            strategyWeights_[(int)(InvmapStrategy::Bidir)]      = child->ChildAs<Float>("bidir", 1_f);
            strategyWeights_[(int)(InvmapStrategy::Lens)]       = child->ChildAs<Float>("lens", 1_f);
            strategyWeights_[(int)(InvmapStrategy::Caustic)]    = child->ChildAs<Float>("caustic", 1_f);
            strategyWeights_[(int)(InvmapStrategy::Multichain)] = child->ChildAs<Float>("multichain", 1_f);
            strategyWeights_[(int)(InvmapStrategy::Identity)]   = child->ChildAs<Float>("identity", 0_f);
            strategyWeights_[(int)(InvmapStrategy::SmallStep)]  = child->ChildAs<Float>("smallstep", 1_f);
            strategyWeights_[(int)(InvmapStrategy::LargeStep)]  = child->ChildAs<Float>("largestep", 1_f);
        }

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

        #if INVERSEMAP_MLTINVMAPFIXED_DEBUG_OUTPUT_TRIANGLE
        // Output triangles
        {
            std::ofstream out("tris.out", std::ios::out | std::ios::trunc);
            for (int i = 0; i < scene->NumPrimitives(); i++)
            {
                const auto* primitive = scene->PrimitiveAt(i);
                const auto* mesh = primitive->mesh;
                if (!mesh) { continue; }
                const auto* ps = mesh->Positions();
                const auto* faces = mesh->Faces();
                for (int fi = 0; fi < primitive->mesh->NumFaces(); fi++)
                {
                    unsigned int vi1 = faces[3 * fi];
                    unsigned int vi2 = faces[3 * fi + 1];
                    unsigned int vi3 = faces[3 * fi + 2];
                    Vec3 p1(primitive->transform * Vec4(ps[3 * vi1], ps[3 * vi1 + 1], ps[3 * vi1 + 2], 1_f));
                    Vec3 p2(primitive->transform * Vec4(ps[3 * vi2], ps[3 * vi2 + 1], ps[3 * vi2 + 2], 1_f));
                    Vec3 p3(primitive->transform * Vec4(ps[3 * vi3], ps[3 * vi3 + 1], ps[3 * vi3 + 2], 1_f));
                    out << p1.x << " " << p1.y << " " << p1.z << " "
                        << p2.x << " " << p2.y << " " << p2.z << " "
                        << p3.x << " " << p3.y << " " << p3.z << " " 
                        << p1.x << " " << p1.y << " " << p1.z << std::endl;
                }
            }
        }
        #endif

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
            
            #pragma region Thread-specific context
            struct Context
            {
                Random rng;
                Film::UniquePtr film{ nullptr, nullptr };
                std::vector<Float> currPS;
            };
            std::vector<Context> contexts(Parallel::GetNumThreads());
            for (auto& ctx : contexts)
            {
                ctx.rng.SetSeed(initRng->NextUInt());
                ctx.film = ComponentFactory::Clone<Film>(film);

                // Initial state
#if 1
                while (true)
                {
                    // Generate initial path with bidirectional path tracing
                    const auto path = [&]() -> boost::optional<Path>
                    {
                        Subpath subpathE;
                        Subpath subpathL;
                        subpathE.SampleSubpathFromEndpoint(scene, &ctx.rng, TransportDirection::EL, numVertices_);
                        subpathL.SampleSubpathFromEndpoint(scene, &ctx.rng, TransportDirection::LE, numVertices_);

                        const int nE = (int)(subpathE.vertices.size());
                        for (int t = 1; t <= nE; t++)
                        {
                            const int nL = (int)(subpathL.vertices.size());
                            const int minS = Math::Max(0, Math::Max(2 - t, numVertices_ - t));
                            const int maxS = Math::Min(nL, numVertices_ - t);
                            for (int s = minS; s <= maxS; s++)
                            {
                                if (s + t != numVertices_) { continue; }
                                Path fullpath;
                                if (!fullpath.ConnectSubpaths(scene, subpathL, subpathE, s, t)) { continue; }
                                if (!fullpath.IsPathType(pathType_)) { continue; }
                                const auto Cstar = fullpath.EvaluateUnweightContribution(scene, s);
                                if (Cstar.Black())
                                {
                                    continue;
                                }
                                return fullpath;
                            }
                        }

                        return boost::none;
                    }();
                    if (!path)
                    {
                        continue;
                    }

                    // Convert the path to the primary sample with cdf(path).
                    const auto ps = InversemapUtils::MapPath2PS(*path, initRng);

                    // Sanity check
                    const auto path2 = InversemapUtils::MapPS2Path(scene, ps);
                    if (!path2)
                    {
                        continue;
                    }
                    const auto f1 = path->EvaluateF(0).Luminance();
                    const auto f2 = path2->EvaluateF(0).Luminance();
                    if (Math::Abs(f1 - f2) > Math::Eps())
                    {
                        continue;
                    }

                    ctx.currPS = ps;
                    break;
                }
#else
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
#endif
            }
            #pragma endregion

            // --------------------------------------------------------------------------------

            #if INVERSEMAP_MLTINVMAPFIXED_DEBUG_LONGEST_REJECTION
            static long long maxReject = 0;
            #endif
            Parallel::For(numMutations_, [&](long long index, int threadid, bool init) -> void
            {
                auto& ctx = contexts[threadid];

                // --------------------------------------------------------------------------------

                const bool accept = [&]() -> bool
                {
                    #pragma region Select mutation strategy

                    const auto strategy = [&]() -> InvmapStrategy
                    {
                        static thread_local const auto StrategyDist = [&]() -> Distribution1D
                        {
                            Distribution1D dist;
                            for (Float w : strategyWeights_) dist.Add(w);
                            dist.Normalize();
                            return dist;
                        }();
                        return (InvmapStrategy)(StrategyDist.Sample(ctx.rng.Next()));
                    }();

                    #pragma endregion

                    // --------------------------------------------------------------------------------

                    if (strategy == InvmapStrategy::SmallStep || strategy == InvmapStrategy::LargeStep)
                    {
                        #pragma region PSSMLT mutations

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
                                propPS.push_back(Perturb(rng, u, 1_f / 256_f, 1_f / 16_f));
                            }

                            return propPS;
                        };

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

                        // --------------------------------------------------------------------------------

                        // Mutate
                        auto propPS = strategy == InvmapStrategy::LargeStep ? LargeStep(ctx.currPS, ctx.rng) : SmallStep(ctx.currPS, ctx.rng);

                        // Map primary samples to paths
                        const auto currP = InversemapUtils::MapPS2Path(scene, ctx.currPS);
                        const auto propP = InversemapUtils::MapPS2Path(scene, propPS);

                        // Immediately rejects if the proposed path is invalid or the dimension changes
                        if (!propP || currP->vertices.size() != propP->vertices.size())
                        {
                            return false;
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
                    }
                    else
                    {
                        #pragma region MLT mutations

                        #pragma region Map to path space
                        auto currP = [&]() -> Path
                        {
                            const auto path = InversemapUtils::MapPS2Path(scene, ctx.currPS);
                            assert(path);
                            assert(!path->EvaluateF(0).Black());
                            assert(path->vertices.size() == numVertices_);
                            return *path;
                        }();
                        #pragma endregion

                        // --------------------------------------------------------------------------------

                        #pragma region Mutate the current path
                        const auto prop = MutationStrategy::Mutate((Strategy)(strategy), scene, ctx.rng, currP);
                        if (!prop)
                        {
                            return false;
                        }
                        #pragma endregion

                        // --------------------------------------------------------------------------------

                        #pragma region MH update
                        {
                            const auto Qxy = MutationStrategy::Q((Strategy)(strategy), scene, currP, prop->p, prop->kd, prop->dL);
                            const auto Qyx = MutationStrategy::Q((Strategy)(strategy), scene, prop->p, currP, prop->kd, prop->dL);
                            Float A = 0_f;
                            if (Qxy <= 0_f || Qyx <= 0_f || std::isnan(Qxy) || std::isnan(Qyx))
                            {
                                A = 0_f;
                            }
                            else
                            {
                                A = Math::Min(1_f, Qyx / Qxy);
                            }
                            if (ctx.rng.Next() < A)
                            {
                                currP = prop->p;
                            }
                            else
                            {
                                // This is critical
                                return false;
                            }
                        }
                        #pragma endregion

                        // --------------------------------------------------------------------------------

                        #pragma region Map to primary sample space
                        const auto ps = InversemapUtils::MapPath2PS(currP, &ctx.rng);
                        const auto currP2 = InversemapUtils::MapPS2Path(scene, ps);
                        if (!currP2 || currP.vertices.size() != currP2->vertices.size() || currP2->EvaluateF(0).Black())
                        {
                            // This sometimes happens due to numerical problem
                            return false;
                        }
                        ctx.currPS = ps;
                        #pragma endregion

                        #pragma endregion
                    }

                    // --------------------------------------------------------------------------------
                    return true;
                }();

                // --------------------------------------------------------------------------------

                #if INVERSEMAP_MLTINVMAPFIXED_DEBUG_LONGEST_REJECTION
                {
                    assert(Parallel::GetNumThreads() == 1);
                    static bool prevIsReject = false;
                    static long long sequencialtReject = 0;
                    if (accept)
                    {
                        prevIsReject = false;
                    }
                    else
                    {
                        if (prevIsReject)
                        {
                            sequencialtReject++;
                        }
                        else
                        {
                            sequencialtReject = 1;
                        }
                        prevIsReject = true;
                        if (sequencialtReject > maxReject)
                        {
                            maxReject = sequencialtReject;
                        }
                    }
                }
                if (maxReject > 10000)
                {
                    __debugbreak();
                }
                #else
                LM_UNUSED(accept);
                #endif

                // --------------------------------------------------------------------------------

                #pragma region Accumulate contribution
                {
                    auto currP = InversemapUtils::MapPS2Path(scene, ctx.currPS);
                    #if INVERSEMAP_MLTINVMAPFIXED_DEBUG_COUNT_OCCURRENCES
                    ctx.film->Splat(currP->RasterPosition(), SPD(1_f));
                    #else
                    const auto currF = currP->EvaluateF(0);
                    if (!currF.Black() && currP->IsPathType(pathType_))
                    {
                        ctx.film->Splat(currP->RasterPosition(), currF * (b / InversemapUtils::ScalarContrb(currF)));
                    }
                    #endif
                }
                #pragma endregion

                // --------------------------------------------------------------------------------

                #if INVERSEMAP_MLTINVMAPFIXED_DEBUG_TRACEPLOT
                {
                    assert(Parallel::GetNumThreads() == 1);
                    static long long count = 0;
                    if (count == 0)
                    {
                        boost::filesystem::remove("traceplot.out");
                    }
                    if (count < 1000)
                    {
                        count++;
                        std::ofstream out("traceplot.out", std::ios::out | std::ios::app);
                        for (const auto& v : ctx.currPS)
                        {
                            out << v << " ";
                        }
                        out << std::endl;
                    }
                }
                #endif
            });

            // --------------------------------------------------------------------------------

            #if INVERSEMAP_MLTINVMAPFIXED_DEBUG_LONGEST_REJECTION
            {
                LM_LOG_INFO("Maximum # of rejection: " + std::to_string(maxReject));
            }
            #endif

            // --------------------------------------------------------------------------------

            #pragma region Gather & Rescale
            film->Clear();
            for (auto& ctx : contexts)
            {
                film->Accumulate(ctx.film.get());
            }
            film->Rescale((Float)(film->Width() * film->Height()) / numMutations_);
            #pragma endregion
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

LM_COMPONENT_REGISTER_IMPL(Renderer_Invmap_MLTInvmapFixed, "renderer::invmap_mltinvmapfixed");

LM_NAMESPACE_END
