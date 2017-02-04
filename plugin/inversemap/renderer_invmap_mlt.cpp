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
#include "inversemaputils.h"
#include <mutex>
#include <boost/filesystem.hpp>

#define INVERSEMAP_MLT_DEBUG_OUTPUT_PER_LENGTH_IMAGE 0
#define INVERSEMAP_MLT_DEBUG_OUTPUT_AVE_ACC 1

LM_NAMESPACE_BEGIN

///! Metropolis light transport
class Renderer_Invmap_MLT final : public Renderer
{
public:

    LM_IMPL_CLASS(Renderer_Invmap_MLT, Renderer);

public:

    int maxNumVertices_;
    long long numMutations_;
    double renderTime_;
    long long numSeedSamples_;
    double seedRenderTime_;
    MLTMutationStrategy mut_;
    std::vector<Float> initStrategyWeights_{ 0_f, 0_f, 0_f, 0_f, 0_f, 0_f, 0_f, 0_f, 0_f };
    std::vector<Float> invS1_{ 0_f, 0_f, 0_f, 0_f, 0_f, 0_f, 0_f, 0_f, 0_f };
    std::vector<Float> invS2_{ 0_f, 0_f, 0_f, 0_f, 0_f, 0_f, 0_f, 0_f, 0_f };
    #if INVERSEMAP_OMIT_NORMALIZATION
    Float normalization_;
    #endif

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
            initStrategyWeights_[(int)(MLTStrategy::BidirFixed)]      = 0_f;
            initStrategyWeights_[(int)(MLTStrategy::Bidir)]           = child->ChildAs<Float>("bidir", 1_f);
            initStrategyWeights_[(int)(MLTStrategy::Lens)]            = child->ChildAs<Float>("lens", 1_f);
            initStrategyWeights_[(int)(MLTStrategy::Caustic)]         = child->ChildAs<Float>("caustic", 1_f);
            initStrategyWeights_[(int)(MLTStrategy::Multichain)]      = child->ChildAs<Float>("multichain", 1_f);
            initStrategyWeights_[(int)(MLTStrategy::ManifoldLens)]    = child->ChildAs<Float>("manifoldlens", 1_f);
            initStrategyWeights_[(int)(MLTStrategy::ManifoldCaustic)] = child->ChildAs<Float>("manifoldcaustic", 1_f);
            initStrategyWeights_[(int)(MLTStrategy::Manifold)]        = child->ChildAs<Float>("manifold", 1_f);
            initStrategyWeights_[(int)(MLTStrategy::Identity)]        = child->ChildAs<Float>("identity", 0_f);
            invS1_[(int)(MLTStrategy::BidirFixed)]      = 0_f;
            invS1_[(int)(MLTStrategy::Bidir)]           = child->ChildAs<Float>("bidir_s1", 256_f);
            invS1_[(int)(MLTStrategy::Lens)]            = child->ChildAs<Float>("lens_s1", 256_f);
            invS1_[(int)(MLTStrategy::Caustic)]         = child->ChildAs<Float>("caustic_s1", 256_f);
            invS1_[(int)(MLTStrategy::Multichain)]      = child->ChildAs<Float>("multichain_s1", 256_f);
            invS1_[(int)(MLTStrategy::ManifoldLens)]    = child->ChildAs<Float>("manifoldlens_s1", 256_f);
            invS1_[(int)(MLTStrategy::ManifoldCaustic)] = child->ChildAs<Float>("manifoldcaustic_s1", 256_f);
            invS1_[(int)(MLTStrategy::Manifold)]        = child->ChildAs<Float>("manifold_s1", 256_f);
            invS1_[(int)(MLTStrategy::Identity)]        = child->ChildAs<Float>("identity_s1", 0_f);
            invS2_[(int)(MLTStrategy::BidirFixed)]      = 0_f;
            invS2_[(int)(MLTStrategy::Bidir)]           = child->ChildAs<Float>("bidir_s2", 16_f);
            invS2_[(int)(MLTStrategy::Lens)]            = child->ChildAs<Float>("lens_s2", 16_f);
            invS2_[(int)(MLTStrategy::Caustic)]         = child->ChildAs<Float>("caustic_s2", 16_f);
            invS2_[(int)(MLTStrategy::Multichain)]      = child->ChildAs<Float>("multichain_s2", 16_f);
            invS2_[(int)(MLTStrategy::ManifoldLens)]    = child->ChildAs<Float>("manifoldlens_s2", 16_f);
            invS2_[(int)(MLTStrategy::ManifoldCaustic)] = child->ChildAs<Float>("manifoldcaustic_s2", 16_f);
            invS2_[(int)(MLTStrategy::Manifold)]        = child->ChildAs<Float>("manifold_s2", 16_f);
            invS2_[(int)(MLTStrategy::Identity)]        = child->ChildAs<Float>("identity_s2", 16_f);
        }
        #if INVERSEMAP_OMIT_NORMALIZATION
        normalization_ = prop->ChildAs<Float>("normalization", 1_f);
        #endif
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
        throw std::runtime_error("TBA");
        #endif
        #pragma endregion

        // --------------------------------------------------------------------------------
        
        #if INVERSEMAP_MLT_DEBUG_OUTPUT_PER_LENGTH_IMAGE
        std::vector<Film::UniquePtr> perLengthFilms;
        std::mutex perLengthFilmMutex;
        for (int i = 0; i < maxNumVertices_ - 1; i++)
        {
            perLengthFilms.push_back(ComponentFactory::Clone<Film>(film));
        }
        #endif

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
                Path currP;
                #if INVERSEMAP_MLT_DEBUG_OUTPUT_AVE_ACC
                long long acceptCount = 0;
                std::vector<long long> acceptCountPerTech;
                std::vector<long long> sampleCountPerTech;
                #endif
            };
            std::vector<Context> contexts(Parallel::GetNumThreads());
            for (auto& ctx : contexts)
            {
                ctx.rng.SetSeed(initRng->NextUInt());
                ctx.film = ComponentFactory::Clone<Film>(film);
                #if INVERSEMAP_MLT_DEBUG_OUTPUT_AVE_ACC
                ctx.acceptCountPerTech.assign(initStrategyWeights_.size(), 0);
                ctx.sampleCountPerTech.assign(initStrategyWeights_.size(), 0);
                #endif

                // Initial state
                while (true)
                {
                    // Generate initial path with bidirectional path tracing
                    const auto path = [&]() -> boost::optional<Path>
                    {
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
                                return fullpath;
                            }
                        }

                        return boost::none;
                    }();
                    if (!path)
                    {
                        continue;
                    }

                    ctx.currP = *path;
                    break;
                }
            }

            // --------------------------------------------------------------------------------

            const auto processed = Parallel::For({ renderTime_ < 0 ? ParallelMode::Samples : ParallelMode::Time, numMutations_, renderTime_ }, [&](long long index, int threadid, bool init) -> void
            {
                auto& ctx = contexts[threadid];

                // --------------------------------------------------------------------------------
                
                struct MutationResult { bool accept; MLTStrategy strategy; };
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
                            if (MLTMutationStrategy::CheckMutatable((MLTStrategy)(i), path))
                            {
                                strategySelectionDist.Add(w);
                            }
                            else
                            {
                                strategySelectionDist.Add(0_f);
                            }
                        }
                        strategySelectionDist.Normalize();
                    };
                    UpdateStrategySelectionDist(ctx.currP);
                    const auto strategy = (MLTStrategy)(strategySelectionDist.Sample(ctx.rng.Next()));
                    #pragma endregion

                    // --------------------------------------------------------------------------------

                    #pragma region Mutate the current path
                    const auto prop = MLTMutationStrategy::Mutate(strategy, scene, ctx.rng, ctx.currP, maxNumVertices_, 1_f / invS1_[(int)strategy], 1_f / invS2_[(int)strategy]);
                    if (!prop)
                    {
                        return { false, strategy };
                    }
                    #pragma endregion

                    // --------------------------------------------------------------------------------

                    #pragma region MH update
                    {
                        const auto Qxy = MLTMutationStrategy::Q(strategy, scene, ctx.currP, prop->p, prop->subspace, maxNumVertices_) * strategySelectionDist.EvaluatePDF((int)(strategy));
                        UpdateStrategySelectionDist(prop->p);
                        const auto Qyx = MLTMutationStrategy::Q(strategy, scene, prop->p, ctx.currP, prop->subspace.Reverse(), maxNumVertices_) * strategySelectionDist.EvaluatePDF((int)(strategy));
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
                            ctx.currP = prop->p;
                        }
                        else
                        {
                            return { false, strategy };
                        }
                    }
                    #pragma endregion

                    // --------------------------------------------------------------------------------

                    return { true, strategy };
                }();

                // --------------------------------------------------------------------------------

                #if INVERSEMAP_MLT_DEBUG_OUTPUT_AVE_ACC
                if (mutationResult.accept) { ctx.acceptCount++; }
                ctx.sampleCountPerTech[(int)(mutationResult.strategy)]++;
                if (mutationResult.accept) { ctx.acceptCountPerTech[(int)(mutationResult.strategy)]++; }
                #else
                LM_UNUSED(mutationResult);
                #endif

                // --------------------------------------------------------------------------------

                #pragma region Accumulate contribution
                {
                    const auto currF = ctx.currP.EvaluateF(0);
                    if (!currF.Black())
                    {
                        const auto rp = ctx.currP.RasterPosition();
                        const auto C = currF * (b / InversemapUtils::ScalarContrb(currF));
                        ctx.film->Splat(rp, C);
                        #if INVERSEMAP_MLT_DEBUG_OUTPUT_PER_LENGTH_IMAGE
                        {
                            std::unique_lock<std::mutex> lock(perLengthFilmMutex);
                            perLengthFilms[ctx.currP.vertices.size() - 2]->Splat(rp, C);
                        }
                        #endif
                    }
                }
                #pragma endregion
            });
            
            // --------------------------------------------------------------------------------

            #if INVERSEMAP_MLT_DEBUG_OUTPUT_AVE_ACC
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

            #pragma region Gather & Rescale
            film->Clear();
            for (auto& ctx : contexts)
            {
                film->Accumulate(ctx.film.get());
            }
            film->Rescale((Float)(film->Width() * film->Height()) / processed);
            #pragma endregion
        }
        #pragma endregion

        // --------------------------------------------------------------------------------
        
        #pragma region Save image
        {
            LM_LOG_INFO("Saving image");
            LM_LOG_INDENTER();
            film->Save(outputPath);
            #if INVERSEMAP_MLT_DEBUG_OUTPUT_PER_LENGTH_IMAGE
            for (int i = 0; i < maxNumVertices_ - 1; i++)
            {
                perLengthFilms[i]->Rescale((Float)(film->Width() * film->Height()) / processed);
                perLengthFilms[i]->Save((boost::filesystem::path(outputPath).remove_filename() / boost::str(boost::format("L%02d") % i)).string());
            }
            #endif
        }
        #pragma endregion
    };

};

LM_COMPONENT_REGISTER_IMPL(Renderer_Invmap_MLT, "renderer::invmap_mlt");

LM_NAMESPACE_END
