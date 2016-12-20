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
#include "multiplexeddensity.h"

LM_NAMESPACE_BEGIN

enum class MMLTInvmapFixed_Strategy : int
{
    // Path space mutations
    Bidir           = (int)(MLTStrategy::Bidir),
    Lens            = (int)(MLTStrategy::Lens),
    Caustic         = (int)(MLTStrategy::Caustic),
    Multichain      = (int)(MLTStrategy::Multichain),
    ManifoldLens    = (int)(MLTStrategy::ManifoldLens),
    ManifoldCaustic = (int)(MLTStrategy::ManifoldCaustic),
    Manifold        = (int)(MLTStrategy::Manifold),
    Identity        = (int)(MLTStrategy::Identity),

    // Primary sample space mutations
    SmallStep,
    LargeStep,
    ChangeTechnique,
};

///! MMLT with fused mutation (fixed path length).
class Renderer_Invmap_MMLTInvmapFixed final : public Renderer
{
public:

    LM_IMPL_CLASS(Renderer_Invmap_MMLTInvmapFixed, Renderer);

public:

    int numVertices_;
    long long numMutations_;
    std::vector<Float> initStrategyWeights_{ 1_f, 1_f, 1_f, 1_f, 1_f, 1_f, 1_f, 1_f, 1_f, 1_f, 1_f };
    #if INVERSEMAP_OMIT_NORMALIZATION
    Float normalization_;
    #endif
    std::string pathType_;

public:

    LM_IMPL_F(Initialize) = [this](const PropertyNode* prop) -> bool
    {
        if (!prop->ChildAs<int>("num_vertices", numVertices_)) return false;
        if (!prop->ChildAs<long long>("num_mutations", numMutations_)) return false;

        {
            LM_LOG_INFO("Loading mutation strategy weights");
            LM_LOG_INDENTER();
            const auto* child = prop->Child("mutation_strategy_weights");
            if (!child)
            {
                LM_LOG_ERROR("Missing 'mutation_strategy_weights'");
                return false;
            }
            initStrategyWeights_[(int)(MMLTInvmapFixed_Strategy::Bidir)]           = child->ChildAs<Float>("bidir", 1_f);
            initStrategyWeights_[(int)(MMLTInvmapFixed_Strategy::Lens)]            = child->ChildAs<Float>("lens", 1_f);
            initStrategyWeights_[(int)(MMLTInvmapFixed_Strategy::Caustic)]         = child->ChildAs<Float>("caustic", 1_f);
            initStrategyWeights_[(int)(MMLTInvmapFixed_Strategy::Multichain)]      = child->ChildAs<Float>("multichain", 1_f);
            initStrategyWeights_[(int)(MMLTInvmapFixed_Strategy::ManifoldLens)]    = child->ChildAs<Float>("manifoldlens", 0_f);
            initStrategyWeights_[(int)(MMLTInvmapFixed_Strategy::ManifoldCaustic)] = child->ChildAs<Float>("manifoldcaustic", 0_f);
            initStrategyWeights_[(int)(MMLTInvmapFixed_Strategy::Manifold)]        = child->ChildAs<Float>("manifold", 1_f);
            initStrategyWeights_[(int)(MMLTInvmapFixed_Strategy::Identity)]        = child->ChildAs<Float>("identity", 0_f);
            initStrategyWeights_[(int)(MMLTInvmapFixed_Strategy::SmallStep)]       = child->ChildAs<Float>("smallstep", 1_f);
            initStrategyWeights_[(int)(MMLTInvmapFixed_Strategy::LargeStep)]       = child->ChildAs<Float>("largestep", 1_f);
            initStrategyWeights_[(int)(MMLTInvmapFixed_Strategy::ChangeTechnique)] = child->ChildAs<Float>("changetechnique", 0_f);
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
        
        #pragma region Compute normalization factor
        #if INVERSEMAP_OMIT_NORMALIZATION
        const auto b = normalization_;
        #else
        throw std::runtime_error("not implemented");
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
                Film::UniquePtr film{ nullptr, nullptr };
                MultiplexedDensity::State curr;
            };
            std::vector<Context> contexts(Parallel::GetNumThreads());
            for (auto& ctx : contexts)
            {
                ctx.rng.SetSeed(initRng->NextUInt());
                ctx.film = ComponentFactory::Clone<Film>(film);

                // Initial state
                while (true)
                {
                    MultiplexedDensity::State state(initRng, numVertices_);
                    const auto path = MultiplexedDensity::InvCDF(state, scene);
                    if (!path)
                    {
                        continue;
                    }
                    if (!path->path.IsPathType(pathType_))
                    {
                        continue;
                    }

                    // Sanity check
                    const auto invS = MultiplexedDensity::CDF(path->path, path->s, scene, initRng);
                    if (!invS)
                    {
                        continue;
                    }
                    const auto path_invS = MultiplexedDensity::InvCDF(*invS, scene);
                    if (!path_invS)
                    {
                        continue;
                    }
                    //const auto C1 = (path->Cstar * path->w).Luminance();
                    const auto C2 = (path_invS->Cstar * path_invS->w).Luminance();
                    //if (path->s != path_invS->s || path->t != path_invS->t || Math::Abs(C1 - C2) > Math::Eps())
                    //{
                    //    continue;
                    //}
                    if (path->s != path_invS->s || path->t != path_invS->t || C2 == 0)
                    {
                        continue;
                    }

                    ctx.curr = state;
                    break;
                }
            }

            // --------------------------------------------------------------------------------

            Parallel::For(numMutations_, [&](long long index, int threadid, bool init) -> void
            {
                auto& ctx = contexts[threadid];

                // --------------------------------------------------------------------------------

                #pragma region Mutation
                const auto accept = [&]() -> bool
                {
                    #pragma region Select mutation strategy
                    const auto strategy = [&]() -> MMLTInvmapFixed_Strategy
                    {
                        const auto StrategyDist = [&]() -> Distribution1D
                        {
                            Distribution1D dist;
                            for (size_t i = 0; i < initStrategyWeights_.size(); i++)
                            {
                                const auto w = initStrategyWeights_[i];
                                if (i <= (int)(MMLTInvmapFixed_Strategy::Identity))
                                {
                                    // Path space mutations
                                    // TODO: Optimize it
                                    const auto currP = MultiplexedDensity::InvCDF(ctx.curr, scene);
                                    if (MLTMutationStrategy::CheckMutatable((MLTStrategy)(i), currP->path)) { dist.Add(w); }
                                    else { dist.Add(0_f); }
                                }
                                else
                                {
                                    // Primary sample space mutations
                                    dist.Add(w);
                                }
                            }
                            dist.Normalize();
                            return dist;
                        }();
                        return (MMLTInvmapFixed_Strategy)(StrategyDist.Sample(ctx.rng.Next()));
                    }();
                    #pragma endregion

                    // --------------------------------------------------------------------------------

                    if (strategy == MMLTInvmapFixed_Strategy::SmallStep || strategy == MMLTInvmapFixed_Strategy::LargeStep || strategy == MMLTInvmapFixed_Strategy::ChangeTechnique)
                    {
                        #pragma region Primary sample space mutations

                        // Mutate
                        auto prop =
                            strategy == MMLTInvmapFixed_Strategy::LargeStep ? ctx.curr.LargeStep(&ctx.rng) :
                            strategy == MMLTInvmapFixed_Strategy::SmallStep ? ctx.curr.SmallStep(&ctx.rng) : ctx.curr.ChangeTechnique(&ctx.rng);

                        // Paths
                        const auto currP = MultiplexedDensity::InvCDF(ctx.curr, scene);
                        const auto propP = MultiplexedDensity::InvCDF(prop, scene);
                        if (!propP)
                        {
                            return false;
                        }
                        
                        // Scalar contributions
                        #if INVERSEMAP_MULTIPLEXED_DENSITY_DEBUG_SIMPLIFY_STRATEGY_SINGLE
                        const auto currC = InversemapUtils::ScalarContrb(currP->Cstar);
                        const auto propC = InversemapUtils::ScalarContrb(propP->Cstar);
                        #else
                        const auto currC = InversemapUtils::ScalarContrb(currP->Cstar * currP->w);
                        const auto propC = InversemapUtils::ScalarContrb(propP->Cstar * propP->w);
                        #endif

                        // MH update
                        const auto A = currC == 0_f ? 1_f : Math::Min(1_f, propC / currC);
                        if (ctx.rng.Next() < A)
                        {
                            ctx.curr.Swap(prop);
                            return true;
                        }

                        return false;
                        #pragma endregion
                    }
                    else
                    {
                        #pragma region Path space mutations
                        
                        #pragma region Map to path space
                        const auto currP = [&]() -> MultiplexedDensity::CachedPath
                        {
                            const auto p = MultiplexedDensity::InvCDF(ctx.curr, scene);
                            assert(p);
                            assert((p->Cstar * p->w).Luminance() > 0);
                            assert(p->path.vertices.size() == numVertices_);
                            return *p;
                        }();
                        #pragma endregion

                        // --------------------------------------------------------------------------------

                        #pragma region Mutate the current path
                        const auto propP = MLTMutationStrategy::Mutate((MLTStrategy)(strategy), scene, ctx.rng, currP.path);
                        if (!propP)
                        {
                            return false;
                        }
                        #pragma endregion

                        // --------------------------------------------------------------------------------
                        
                        #pragma region MH update
                        {
                            const auto Qxy = MLTMutationStrategy::Q((MLTStrategy)(strategy), scene, currP.path, propP->p, propP->subspace);
                            const auto Qyx = MLTMutationStrategy::Q((MLTStrategy)(strategy), scene, propP->p, currP.path, propP->subspace);
                            Float A = 0_f;
                            if (Qxy <= 0_f || Qyx <= 0_f || std::isnan(Qxy) || std::isnan(Qyx))
                            {
                                A = 0_f;
                            }
                            else
                            {
                                #if INVERSEMAP_MULTIPLEXED_DENSITY_DEBUG_SIMPLIFY_STRATEGY_SINGLE
                                A = Math::Min(1_f, Qyx / Qxy);
                                #else
                                // Reject if proposed path is not samplable by current technique
                                if (propP->p.EvaluatePathPDF(scene, currP.s).v == 0_f)
                                {
                                    return false;
                                }

                                const auto wx = currP.w;
                                const auto wy = propP->p.EvaluateMISWeight(scene, currP.s);
                                if (wx <= 0 || wy <= 0)
                                {
                                    A = 0_f;
                                }
                                else
                                {
                                    A = Math::Min(1_f, (Qyx * wy) / (Qxy * wx));
                                }
                                #endif
                            }
                            if (ctx.rng.Next() < A)
                            {
                                // Map to primary sample space
                                const auto propInvS = MultiplexedDensity::CDF(propP->p, currP.s, scene, &ctx.rng);
                                if (!propInvS)
                                {
                                    return false;
                                }

                                // Sanity check
                                const auto path_propInvS = MultiplexedDensity::InvCDF(*propInvS, scene);
                                if (!path_propInvS)
                                {
                                    return false;
                                }
                                //const auto C1 = (currP.Cstar * currP.w).Luminance();
                                const auto C2 = (path_propInvS->Cstar * path_propInvS->w).Luminance();
                                //if (currP.s != path_propInvS->s || currP.t != path_propInvS->t || Math::Abs(C1 - C2) > Math::Eps())
                                //{
                                //    return false;
                                //}
                                if (currP.s != path_propInvS->s || currP.t != path_propInvS->t || C2 == 0)
                                {
                                    return false;
                                }

                                ctx.curr = *propInvS;
                                return true;
                            }

                            return false;
                        }
                        #pragma endregion

                        #pragma endregion
                    }

                    // --------------------------------------------------------------------------------
                    LM_UNREACHABLE();
                    return true;
                }();
                LM_UNUSED(accept);
                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Accumulate contribution
                {
                    const auto p = MultiplexedDensity::InvCDF(ctx.curr, scene);
                    assert(p);
                    if (p->path.IsPathType(pathType_))
                    {
                        const auto C = p->Cstar * p->w;
                        const auto I = InversemapUtils::ScalarContrb(C);
                        ctx.film->Splat(p->path.RasterPosition(), C * (b / I));
                    }
                }
                #pragma endregion
            });

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

        #if INVERSEMAP_DEBUG_MLT_MANIFOLDWALK_STAT
        MLTMutationStrategy::PrintStat();
        #endif

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

LM_COMPONENT_REGISTER_IMPL(Renderer_Invmap_MMLTInvmapFixed, "renderer::invmap_mmltinvmapfixed");

LM_NAMESPACE_END
