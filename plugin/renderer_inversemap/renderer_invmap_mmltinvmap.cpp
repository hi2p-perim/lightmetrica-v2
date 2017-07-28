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
#include "debugio.h"
#include <regex>
#include <chrono>
#include <cereal/archives/json.hpp>
#include <cereal/types/vector.hpp>

#define INVERSEMAP_MMLTINVMAP_DEBUG_OUTPUT_AVE_ACC 1
#define INVERSEMAP_MMLTINVMAP_MEASURE_TRANSITION_TIME 1

#define INVERSEMAP_MMLTINVMAP_DEBUG_IO 0

LM_NAMESPACE_BEGIN

enum class MMLTInvmap_Strategy : int
{
    // Path space mutations
    BidirFixed      = (int)(MLTStrategy::BidirFixed),
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

///! MMLT with fused mutation
class Renderer_Invmap_MMLTInvmap final : public Renderer
{
public:

    LM_IMPL_CLASS(Renderer_Invmap_MMLTInvmap, Renderer);

public:

    int maxNumVertices_;
    long long numMutations_;
    double renderTime_;
    long long numSeedSamples_;
    double seedRenderTime_;
    std::vector<Float> initStrategyWeights_{ 0_f, 0_f, 0_f, 0_f, 0_f, 0_f, 0_f, 0_f, 0_f, 0_f, 0_f, 0_f };
    std::vector<Float> invS1_{ 0_f, 0_f, 0_f, 0_f, 0_f, 0_f, 0_f, 0_f, 0_f, 0_f, 0_f, 0_f };
    std::vector<Float> invS2_{ 0_f, 0_f, 0_f, 0_f, 0_f, 0_f, 0_f, 0_f, 0_f, 0_f, 0_f, 0_f };

public:

    LM_IMPL_F(Initialize) = [this](const PropertyNode* prop) -> bool
    {
        if (!prop->ChildAs<int>("max_num_vertices", maxNumVertices_)) return false;
        numMutations_ = prop->ChildAs<long long>("num_mutations", 0);
        renderTime_ = prop->ChildAs<double>("render_time", -1);
        numSeedSamples_ = prop->ChildAs<long long>("num_seed_samples", 0);
        seedRenderTime_ = prop->ChildAs<double>("seed_render_time", -1);
        {
            LM_LOG_INFO("Loading mutation strategy weights");
            LM_LOG_INDENTER();
            const auto* child = prop->Child("mutation_strategy_weights");
            if (!child)
            {
                LM_LOG_ERROR("Missing 'mutation_strategy_weights'");
                return false;
            }
            initStrategyWeights_[(int)(MMLTInvmap_Strategy::BidirFixed)]      = child->ChildAs<Float>("bidir", 1_f);
            initStrategyWeights_[(int)(MMLTInvmap_Strategy::Bidir)]           = 0_f;
            initStrategyWeights_[(int)(MMLTInvmap_Strategy::Lens)]            = child->ChildAs<Float>("lens", 1_f);
            initStrategyWeights_[(int)(MMLTInvmap_Strategy::Caustic)]         = child->ChildAs<Float>("caustic", 1_f);
            initStrategyWeights_[(int)(MMLTInvmap_Strategy::Multichain)]      = child->ChildAs<Float>("multichain", 1_f);
            initStrategyWeights_[(int)(MMLTInvmap_Strategy::ManifoldLens)]    = child->ChildAs<Float>("manifoldlens", 1_f);
            initStrategyWeights_[(int)(MMLTInvmap_Strategy::ManifoldCaustic)] = child->ChildAs<Float>("manifoldcaustic", 1_f);
            initStrategyWeights_[(int)(MMLTInvmap_Strategy::Manifold)]        = child->ChildAs<Float>("manifold", 1_f);
            initStrategyWeights_[(int)(MMLTInvmap_Strategy::Identity)]        = child->ChildAs<Float>("identity", 0_f);
            initStrategyWeights_[(int)(MMLTInvmap_Strategy::SmallStep)]       = child->ChildAs<Float>("smallstep", 1_f);
            initStrategyWeights_[(int)(MMLTInvmap_Strategy::LargeStep)]       = child->ChildAs<Float>("largestep", 1_f);
            initStrategyWeights_[(int)(MMLTInvmap_Strategy::ChangeTechnique)] = child->ChildAs<Float>("changetechnique", 0_f);
            invS1_[(int)(MMLTInvmap_Strategy::BidirFixed)]      = child->ChildAs<Float>("bidir_s1", 256_f);
            invS1_[(int)(MMLTInvmap_Strategy::Bidir)]           = 0_f;
            invS1_[(int)(MMLTInvmap_Strategy::Lens)]            = child->ChildAs<Float>("lens_s1", 256_f);
            invS1_[(int)(MMLTInvmap_Strategy::Caustic)]         = child->ChildAs<Float>("caustic_s1", 256_f);
            invS1_[(int)(MMLTInvmap_Strategy::Multichain)]      = child->ChildAs<Float>("multichain_s1", 256_f);
            invS1_[(int)(MMLTInvmap_Strategy::ManifoldLens)]    = child->ChildAs<Float>("manifoldlens_s1", 256_f);
            invS1_[(int)(MMLTInvmap_Strategy::ManifoldCaustic)] = child->ChildAs<Float>("manifoldcaustic_s1", 256_f);
            invS1_[(int)(MMLTInvmap_Strategy::Manifold)]        = child->ChildAs<Float>("manifold_s1", 256_f);
            invS1_[(int)(MMLTInvmap_Strategy::Identity)]        = child->ChildAs<Float>("identity_s1", 256_f);
            invS1_[(int)(MMLTInvmap_Strategy::SmallStep)]       = child->ChildAs<Float>("smallstep_s1", 256_f);
            invS1_[(int)(MMLTInvmap_Strategy::LargeStep)]       = 0_f;
            invS1_[(int)(MMLTInvmap_Strategy::ChangeTechnique)] = child->ChildAs<Float>("changetechnique_s1", 256_f);
            invS2_[(int)(MMLTInvmap_Strategy::BidirFixed)]      = child->ChildAs<Float>("bidir_s2", 16_f);
            invS2_[(int)(MMLTInvmap_Strategy::Bidir)]           = 0_f;
            invS2_[(int)(MMLTInvmap_Strategy::Lens)]            = child->ChildAs<Float>("lens_s2", 16_f);
            invS2_[(int)(MMLTInvmap_Strategy::Caustic)]         = child->ChildAs<Float>("caustic_s2", 16_f);
            invS2_[(int)(MMLTInvmap_Strategy::Multichain)]      = child->ChildAs<Float>("multichain_s2", 16_f);
            invS2_[(int)(MMLTInvmap_Strategy::ManifoldLens)]    = child->ChildAs<Float>("manifoldlens_s2", 16_f);
            invS2_[(int)(MMLTInvmap_Strategy::ManifoldCaustic)] = child->ChildAs<Float>("manifoldcaustic_s2", 16_f);
            invS2_[(int)(MMLTInvmap_Strategy::Manifold)]        = child->ChildAs<Float>("manifold_s2", 16_f);
            invS2_[(int)(MMLTInvmap_Strategy::Identity)]        = child->ChildAs<Float>("identity_s2", 16_f);
            invS2_[(int)(MMLTInvmap_Strategy::SmallStep)]       = child->ChildAs<Float>("smallstep_s2", 16_f);
            invS2_[(int)(MMLTInvmap_Strategy::LargeStep)]       = 0_f;
            invS2_[(int)(MMLTInvmap_Strategy::ChangeTechnique)] = child->ChildAs<Float>("changetechnique_s2", 16_f);
        }
        return true;
    };

    LM_IMPL_F(Render) = [this](const Scene* scene_, Random* initRng, const std::string& outputPath) -> void
    {
        #if INVERSEMAP_MMLTINVMAP_DEBUG_IO
        DebugIO::Run();
        #endif

        // --------------------------------------------------------------------------------

        const auto* scene = static_cast<const Scene3*>(scene_);
        auto* film = static_cast<const Sensor*>(scene->GetSensor()->emitter)->GetFilm();

        // --------------------------------------------------------------------------------
        
        #if INVERSEMAP_MMLTINVMAP_DEBUG_IO
        LM_LOG_DEBUG("triangle_vertices");
        {
            DebugIO::Wait();

            std::vector<double> vs;
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
                    for (int j = 0; j < 3; j++) vs.push_back(p1[j]);
                    for (int j = 0; j < 3; j++) vs.push_back(p2[j]);
                    for (int j = 0; j < 3; j++) vs.push_back(p3[j]);
                }
            }
            
            std::stringstream ss;
            {
                cereal::JSONOutputArchive oa(ss);
                oa(vs);
            }

            DebugIO::Output("triangle_vertices", ss.str());
            DebugIO::Wait();
        }
        #endif

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

            // Thread-specific context
            struct Context
            {
                Random rng;
                Film::UniquePtr film{ nullptr, nullptr };
                struct CachedState
                {
                    MultiplexedDensity::State state;            // Current state in multipleded primary sample space
                    MultiplexedDensity::CachedPath path;        // Cached path
                };
                std::vector<CachedState> curr;
                #if INVERSEMAP_MMLTINVMAP_DEBUG_OUTPUT_AVE_ACC
                long long acceptCount = 0;
                std::vector<long long> acceptCountPerTech;
                std::vector<long long> sampleCountPerTech;
                #endif
                #if INVERSEMAP_MMLTINVMAP_MEASURE_TRANSITION_TIME
                double transitionTime = 0;
                long long transitionCount = 0;
                long long sanitycheckCount = 0;
                long long sanitycheckFailureCount = 0;
                long long sanitycheckFailureCount1 = 0;
                long long sanitycheckFailureCount2 = 0;
                #endif
            };
            std::vector<Context> contexts(Parallel::GetNumThreads());
            for (auto& ctx : contexts)
            {
                ctx.rng.SetSeed(initRng->NextUInt());
                ctx.film = ComponentFactory::Clone<Film>(film);
                ctx.curr.assign(maxNumVertices_ - 1, Context::CachedState());
                #if INVERSEMAP_MMLTINVMAP_DEBUG_OUTPUT_AVE_ACC
                ctx.acceptCountPerTech.assign(initStrategyWeights_.size(), 0);
                ctx.sampleCountPerTech.assign(initStrategyWeights_.size(), 0);
                #endif

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
                        MultiplexedDensity::State state(initRng, k + 2);
                        const auto path = MultiplexedDensity::InvCDF(state, scene);
                        if (!path)
                        {
                            continue;
                        }

                        // --------------------------------------------------------------------------------

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
                        const auto C2 = (path_invS->Cstar * path_invS->w).Luminance();
                        if (path->s != path_invS->s || path->t != path_invS->t || C2 == 0)
                        {
                            continue;
                        }

                        // --------------------------------------------------------------------------------

                        //LM_LOG_INFO("Found with iter = " + std::to_string(i));
                        ctx.curr[k].state = std::move(state);
                        ctx.curr[k].path  = std::move(*path);
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
                struct MutationResult
                {
                    bool accept;
                    MMLTInvmap_Strategy strategy;
                };
                const auto mutationResult = [&]() -> MutationResult
                {
                    #pragma region Select mutation strategy
                    Distribution1D strategySelectionDist;
                    const auto UpdateStrategySelectionDist = [&](const Path& path) -> void
                    {
                        strategySelectionDist.Clear();
                        for (size_t i = 0; i < initStrategyWeights_.size(); i++)
                        {
                            const auto w = initStrategyWeights_[i];
                            if (i <= (int)(MMLTInvmap_Strategy::Identity))
                            {
                                // Path space mutations
                                if (MLTMutationStrategy::CheckMutatable((MLTStrategy)(i), path)) { strategySelectionDist.Add(w); }
                                else { strategySelectionDist.Add(0_f); }
                            }
                            else
                            {
#if 1
                                strategySelectionDist.Add(w);
#else
                                if (i == (int)MMLTInvmap_Strategy::SmallStep)
                                {
                                    // Give chance to mutate with ME for SDS paths
                                    const auto ContainsSDS = [&]() -> bool
                                    {
                                        const auto type = path.PathType();
                                        thread_local std::regex reg(R"x(^LSDSE$)x");
                                        std::smatch match;
                                        if (!std::regex_match(type, match, reg)) { return false; }
                                        return true;
                                    };
                                    if (ContainsSDS())
                                    {
                                        strategySelectionDist.Add(0_f);
                                    }
                                    else
                                    {
                                        strategySelectionDist.Add(w);
                                    }
                                }
                                else
                                {
                                    // Primary sample space mutations
                                    strategySelectionDist.Add(w);
                                }
#endif
                            }
                        }
                        strategySelectionDist.Normalize();
                    };
                    UpdateStrategySelectionDist(ctx.curr[k].path.path);
                    const auto strategy = (MMLTInvmap_Strategy)(strategySelectionDist.Sample(ctx.rng.Next()));
                    #pragma endregion

                    // --------------------------------------------------------------------------------

                    if (strategy == MMLTInvmap_Strategy::SmallStep || strategy == MMLTInvmap_Strategy::LargeStep || strategy == MMLTInvmap_Strategy::ChangeTechnique)
                    {
                        #pragma region Primary sample space mutations
                        {
                            // Mutate
                            auto prop =
                                strategy == MMLTInvmap_Strategy::LargeStep ? ctx.curr[k].state.LargeStep(&ctx.rng) :
                                strategy == MMLTInvmap_Strategy::SmallStep ? ctx.curr[k].state.SmallStep(&ctx.rng, 1_f / invS1_[(int)MMLTInvmap_Strategy::SmallStep], 1_f / invS2_[(int)MMLTInvmap_Strategy::SmallStep]) :
                                ctx.curr[k].state.ChangeTechnique(&ctx.rng, 1_f / invS1_[(int)MMLTInvmap_Strategy::ChangeTechnique], 1_f / invS2_[(int)MMLTInvmap_Strategy::ChangeTechnique]);

                            // Paths
                            const auto& currP = ctx.curr[k].path;
                            const auto  propP = MultiplexedDensity::InvCDF(prop, scene);
                            if (!propP)
                            {
                                return{ false, strategy };
                            }

                            // Scalar contributions
                            const auto currC = InversemapUtils::ScalarContrb(currP.Cstar * currP.w);
                            const auto propC = InversemapUtils::ScalarContrb(propP->Cstar * propP->w);

                            // MH update
                            const auto A = currC == 0_f ? 1_f : Math::Min(1_f, propC / currC);
                            if (ctx.rng.Next() < A)
                            {
                                ctx.curr[k].state = prop;
                                ctx.curr[k].path = *propP;
                                return{ true, strategy };
                            }

                            return{ false, strategy };
                        }
                        #pragma endregion
                    }
                    else
                    {
                        #pragma region Path space mutations
                        {
                            #pragma region Map to path space
                            const auto& currP = ctx.curr[k].path;
                            #pragma endregion

                            // --------------------------------------------------------------------------------

                            #pragma region Mutate the current path
                            const auto propP = MLTMutationStrategy::Mutate((MLTStrategy)(strategy), scene, ctx.rng, currP.path, maxNumVertices_, 1_f / invS1_[(int)strategy], 1_f / invS2_[(int)strategy]);
                            if (!propP)
                            {
                                return { false, strategy };
                            }
                            #pragma endregion

                            // --------------------------------------------------------------------------------

                            #pragma region MH update
                            {
                                const auto Qxy = MLTMutationStrategy::Q((MLTStrategy)(strategy), scene, currP.path, propP->p, propP->subspace, maxNumVertices_) * strategySelectionDist.EvaluatePDF((int)(strategy));
                                UpdateStrategySelectionDist(propP->p);
                                const auto Qyx = MLTMutationStrategy::Q((MLTStrategy)(strategy), scene, propP->p, currP.path, propP->subspace.Reverse(), maxNumVertices_) * strategySelectionDist.EvaluatePDF((int)(strategy));
                                
                                Float A = 0_f;
                                if (Qxy <= 0_f || Qyx <= 0_f || std::isnan(Qxy) || std::isnan(Qyx))
                                {
                                    A = 0_f;
                                }
                                else
                                {
                                    // Reject if proposed path is not samplable by current technique
                                    if (propP->p.EvaluatePathPDF(scene, currP.s).v == 0_f)
                                    {
                                        return { false, strategy };
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
                                }

                                if (ctx.rng.Next() < A)
                                {
                                    // Map to primary sample space
                                    #if INVERSEMAP_MMLTINVMAP_MEASURE_TRANSITION_TIME
                                    const auto T1 = std::chrono::high_resolution_clock::now();
                                    #endif
                                    const auto propInvS = MultiplexedDensity::CDF(propP->p, currP.s, scene, &ctx.rng);
                                    #if INVERSEMAP_MMLTINVMAP_MEASURE_TRANSITION_TIME
                                    const auto T2 = std::chrono::high_resolution_clock::now();
                                    ctx.transitionTime += (double)(std::chrono::duration_cast<std::chrono::milliseconds>(T2 - T1).count()) / 1000.0;
                                    ctx.transitionCount++;
                                    #endif
                                    if (!propInvS)
                                    {
                                        return{ false, strategy };
                                    }

                                    // Sanity check
                                    const auto path_propInvS = MultiplexedDensity::InvCDF(*propInvS, scene);
                                    #if INVERSEMAP_MMLTINVMAP_MEASURE_TRANSITION_TIME
                                    const auto T3 = std::chrono::high_resolution_clock::now();
                                    ctx.transitionTime += (double)(std::chrono::duration_cast<std::chrono::milliseconds>(T3 - T2).count()) / 1000.0;
                                    ctx.sanitycheckCount++;
                                    #endif
                                    if (!path_propInvS)
                                    {
                                        #if INVERSEMAP_MMLTINVMAP_DEBUG_IO
                                        {
                                            LM_LOG_DEBUG("path1");
                                            DebugIO::Wait();
                                            std::vector<double> vs;
                                            for (const auto& v : propP->p.vertices) for (int i = 0; i < 3; i++) { vs.push_back(v.geom.p[i]); }
                                            std::stringstream ss; { cereal::JSONOutputArchive oa(ss); oa(vs); }
                                            DebugIO::Output("path1", ss.str());
                                        }
                                        Subpath subpathE;
                                        Subpath subpathL;
                                        subpathE.vertices.clear();
                                        subpathL.vertices.clear();
                                        subpathE.SampleSubpathWithPrimarySamples(scene, propInvS->usE_, TransportDirection::EL, propInvS->numVertices_);
                                        subpathL.SampleSubpathWithPrimarySamples(scene, propInvS->usL_, TransportDirection::LE, propInvS->numVertices_);
                                        {
                                            LM_LOG_DEBUG("path2");
                                            DebugIO::Wait();
                                            std::vector<double> vs;
                                            for (const auto& v : subpathE.vertices) for (int i = 0; i < 3; i++) { vs.push_back(v.geom.p[i]); }
                                            std::stringstream ss; { cereal::JSONOutputArchive oa(ss); oa(vs); }
                                            DebugIO::Output("path2", ss.str());
                                        }
                                        {
                                            LM_LOG_DEBUG("path3");
                                            DebugIO::Wait();
                                            std::vector<double> vs;
                                            for (const auto& v : subpathL.vertices) for (int i = 0; i < 3; i++) { vs.push_back(v.geom.p[i]); }
                                            std::stringstream ss; { cereal::JSONOutputArchive oa(ss); oa(vs); }
                                            DebugIO::Output("path3", ss.str());
                                        }
                                        #endif

                                        #if INVERSEMAP_MMLTINVMAP_MEASURE_TRANSITION_TIME
                                        ctx.sanitycheckFailureCount++;
                                        ctx.sanitycheckFailureCount1++;
                                        #endif
                                        return { false, strategy };
                                    }
                                    const auto C2 = (path_propInvS->Cstar * path_propInvS->w).Luminance();
                                    if (currP.s != path_propInvS->s || currP.t != path_propInvS->t || C2 == 0)
                                    {
                                        #if INVERSEMAP_MMLTINVMAP_MEASURE_TRANSITION_TIME
                                        ctx.sanitycheckFailureCount++;
                                        ctx.sanitycheckFailureCount2++;
                                        #endif
                                        return { false, strategy };
                                    }

                                    // Update state
                                    ctx.curr[k].state = *propInvS;
                                    ctx.curr[k].path  = *path_propInvS;
                                    return{ true, strategy };
                                }

                                return{ false, strategy };
                            }
                            #pragma endregion
                        }
                        #pragma endregion
                    }

                    // --------------------------------------------------------------------------------
                    LM_UNREACHABLE();
                    return { false, strategy };
                }();
                #pragma endregion

                // --------------------------------------------------------------------------------

                #if INVERSEMAP_MMLTINVMAP_DEBUG_OUTPUT_AVE_ACC
                if (mutationResult.accept) { ctx.acceptCount++; }
                ctx.sampleCountPerTech[(int)(mutationResult.strategy)]++;
                if (mutationResult.accept) { ctx.acceptCountPerTech[(int)(mutationResult.strategy)]++; }
                #else
                LM_UNUSED(mutationResult);
                #endif

                // --------------------------------------------------------------------------------

                #pragma region Accumulate contribution
                {
                    const auto& p = ctx.curr[k].path;
                    const auto  C = p.Cstar * p.w;
                    const auto  I = InversemapUtils::ScalarContrb(C);
                    ctx.film->Splat(p.path.RasterPosition(), C * (b[k] / I) / pathLengthDist.EvaluatePDF(k));
                }
                #pragma endregion
            });

            // --------------------------------------------------------------------------------

            #if INVERSEMAP_MMLTINVMAP_DEBUG_OUTPUT_AVE_ACC
            {
                long long sum = 0;
                for (auto& ctx : contexts) { sum += ctx.acceptCount; }
                const double ave = (double)sum / processed;
                LM_LOG_INFO(boost::str(boost::format("Ave. acceptance ratio: %.5f (%d / %d)") % ave % sum % processed));
            }
            {
                LM_LOG_INFO("Ave. acceptance ratio per strategy");
                LM_LOG_INDENTER();
                for (size_t i = 0; i < initStrategyWeights_.size(); i++)
                {
                    long long acceptCount = 0;
                    long long sampleCount = 0;
                    for (const auto& ctx : contexts)
                    {
                        acceptCount += ctx.acceptCountPerTech[i];
                        sampleCount += ctx.sampleCountPerTech[i];
                    }
                    if (sampleCount > 0)
                    {
                        const double ave = (double)acceptCount / sampleCount;
                        LM_LOG_INFO(boost::str(boost::format("%02d: %.5f (%d / %d)") % i % ave % acceptCount % sampleCount));
                    }
                    else
                    {
                        LM_LOG_INFO(boost::str(boost::format("%02d: N/A") % i));
                    }
                }
            }
            #endif

            // --------------------------------------------------------------------------------

            #if INVERSEMAP_MMLTINVMAP_MEASURE_TRANSITION_TIME
            {
                double sum = 0;
                for (auto& ctx : contexts) { sum += ctx.transitionTime; }
                LM_LOG_INFO(boost::str(boost::format("Transition time: %.5f") % sum));
            }
            {
                long long sum = 0;
                for (auto& ctx : contexts) { sum += ctx.transitionCount; }
                const double ave = (double)sum / processed;
                LM_LOG_INFO(boost::str(boost::format("Expected transition: %.5f (%d / %d)") % ave % sum % processed));
            }
            {
                long long sum = 0;
                long long sumFailures = 0;
                long long sumFailures1 = 0;
                long long sumFailures2 = 0;
                for (auto& ctx : contexts)
                {
                    sum += ctx.sanitycheckCount;
                    sumFailures += ctx.sanitycheckFailureCount;
                    sumFailures1 += ctx.sanitycheckFailureCount1;
                    sumFailures2 += ctx.sanitycheckFailureCount2;
                }

                {
                    const double ave = (double)sum / processed;
                    LM_LOG_INFO(boost::str(boost::format("Expected sanity checks: %.5f (%d / %d)") % ave % sum % processed));
                }
                {
                    const double ave = (double)sumFailures / sum;
                    LM_LOG_INFO(boost::str(boost::format("Expected failure cases in sanity checks: %.5f (%d / %d)") % ave % sumFailures % sum));
                }
                {
                    LM_LOG_INDENTER();
                    {
                        const double ave = (double)sumFailures1 / sum;
                        LM_LOG_INFO(boost::str(boost::format("1: %.5f (%d / %d)") % ave % sumFailures1 % sum));
                    }
                    {
                        const double ave = (double)sumFailures2 / sum;
                        LM_LOG_INFO(boost::str(boost::format("2: %.5f (%d / %d)") % ave % sumFailures2 % sum));
                    }
                }
            }
            #endif

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

        // --------------------------------------------------------------------------------
        
        #if INVERSEMAP_MMLTINVMAP_DEBUG_IO
        DebugIO::Stop();
        #endif
    };

};


LM_COMPONENT_REGISTER_IMPL(Renderer_Invmap_MMLTInvmap, "renderer::invmap_mmltinvmap");

LM_NAMESPACE_END

