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

#define INVERSEMAP_PSSMLT_DEBUG_SIMPLIFY_BDPT 0

LM_NAMESPACE_BEGIN

class PSSMLTState
{
private:

    int maxNumVertices_;
    std::vector<Float> usL_;     // For light subpath
    std::vector<Float> usE_;     // For eye subpath

public:

    PSSMLTState() {}

    PSSMLTState(Random* rng, int maxNumVertices)
        : maxNumVertices_(maxNumVertices)
    {
        const auto numStates = maxNumVertices * 3;
        usL_.assign(numStates, 0_f);
        usE_.assign(numStates, 0_f);
        for (auto& u : usE_) u = rng->Next();
        for (auto& u : usL_) u = rng->Next();
    }

    PSSMLTState(const PSSMLTState& o)
        : maxNumVertices_(o.maxNumVertices_)
        , usL_(o.usL_)
        , usE_(o.usE_)
    {}

public:

    auto Swap(PSSMLTState& o) -> void
    {
        assert(maxNumVertices_ == o.maxNumVertices_);
        usL_.swap(o.usL_);
        usE_.swap(o.usE_);
    }

public:

    // Large step mutation
    auto LargeStep(Random* rng) const -> PSSMLTState
    {
        PSSMLTState next(*this);
        for (auto& u : next.usE_) u = rng->Next();
        for (auto& u : next.usL_) u = rng->Next();
        return next;
    }

    // Small step mutation
    auto SmallStep(Random* rng) const -> PSSMLTState
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

        const auto s1 = 1_f / 256_f;
        const auto s2 = 1_f / 16_f;

        PSSMLTState next(*this);
        for (size_t i = 0; i < usE_.size(); i++) next.usE_[i] = Perturb(*rng, usE_[i], s1, s2);
        for (size_t i = 0; i < usL_.size(); i++) next.usL_[i] = Perturb(*rng, usL_[i], s1, s2);

        return next;
    }

    // Map a primary sample to a set of paths. Returns empty vector if failed.
    struct CachedPaths
    {
        struct CachedPath
        {
            int s, t;
            Path path;
            SPD Cstar;      // Caches unweighed contribution
            Float w;        // Caches MIS weight
        };
        std::vector<CachedPath> ps;
        auto ScalarContrb() const -> Float
        {
            SPD C;
            for (const auto& p : ps) C += p.Cstar * p.w;
            return InversemapUtils::ScalarContrb(C);
        }
    };
    auto InvCDF(const Scene3* scene) const -> CachedPaths
    {
        Subpath subpathE;
        Subpath subpathL;
        subpathE.SampleSubpathWithPrimarySamples(scene, usE_, TransportDirection::EL, maxNumVertices_);
        subpathL.SampleSubpathWithPrimarySamples(scene, usL_, TransportDirection::LE, maxNumVertices_);

        CachedPaths paths;
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

                CachedPaths::CachedPath p;
                p.s = s;
                p.t = t;
                if (!p.path.ConnectSubpaths(scene, subpathL, subpathE, s, t)) { continue; }

                p.Cstar = p.path.EvaluateUnweightContribution(scene, s);
                if (p.Cstar.Black())
                {
                    continue;
                }

                p.w = p.path.EvaluateMISWeight(scene, s);
                paths.ps.push_back(std::move(p));
            }
        }

        return paths;
    }

};

///! Primary sample space metropolis light transport (BDPT path sampler)
class Renderer_Invmap_PSSMLT final : public Renderer
{
public:

    LM_IMPL_CLASS(Renderer_Invmap_PSSMLT, Renderer);

public:
    
    int maxNumVertices_;
    long long numMutations_;
    double renderTime_;
    long long numSeedSamples_;
    double seedRenderTime_;
    Float largeStepProb_;
    #if INVERSEMAP_OMIT_NORMALIZATION
    Float normalization_;
    #endif
    
    Float s1_;
    Float s2_;
    
public:

    LM_IMPL_F(Initialize) = [this](const PropertyNode* prop) -> bool
    {
        if (!prop->ChildAs<int>("max_num_vertices", maxNumVertices_)) return false;
        numMutations_ = prop->ChildAs<long long>("num_mutations", 0);
        renderTime_ = prop->ChildAs<double>("render_time", -1);
        numSeedSamples_ = prop->ChildAs<long long>("num_seed_samples", 0);
        seedRenderTime_ = prop->ChildAs<double>("seed_render_time", -1);
        largeStepProb_ = prop->ChildAs<Float>("large_step_prob", 0.5_f);
        #if INVERSEMAP_OMIT_NORMALIZATION
        normalization_ = prop->ChildAs<Float>("normalization", 1_f);
        #endif
        return true;
    };

    LM_IMPL_F(Render) = [this](const Scene* scene_, Random* initRng, const std::string& outputPath) -> void
    {
        const auto* scene = static_cast<const Scene3*>(scene_);
        auto* film = static_cast<const Sensor*>(scene->GetSensor()->emitter)->GetFilm();

        // --------------------------------------------------------------------------------

        #pragma region Compute normalization factor
        #if INVERSEMAP_OMIT_NORMALIZATION
        const auto b = normalization_;
        #else
        throw std::runtime_error("TBA");
        #endif

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
                PSSMLTState currState;
            };
            std::vector<Context> contexts(Parallel::GetNumThreads());
            for (auto& ctx : contexts)
            {
                ctx.rng.SetSeed(initRng->NextUInt());
                ctx.film = ComponentFactory::Clone<Film>(film);

                // Initial state
                while (true)
                {
                    // Generate initial path with bidirectional path tracing
                    PSSMLTState state(initRng, maxNumVertices_);
                    const auto paths = state.InvCDF(scene);
                    if (paths.ps.empty())
                    {
                        continue;
                    }

                    ctx.currState = std::move(state);
                    break;
                }
            }

            // --------------------------------------------------------------------------------

            const auto processed = Parallel::For({ renderTime_ < 0 ? ParallelMode::Samples : ParallelMode::Time, numMutations_, renderTime_ }, [&](long long index, int threadid, bool init) -> void
            {
                auto& ctx = contexts[threadid];

                // --------------------------------------------------------------------------------

                #pragma region Mutation in primary sample space
                {
                    // Mutate
                    auto propState = ctx.rng.Next() < largeStepProb_
                        ? ctx.currState.LargeStep(&ctx.rng)
                        : ctx.currState.SmallStep(&ctx.rng);

                    // Paths
                    const auto currPs = ctx.currState.InvCDF(scene);
                    const auto propPs = propState.InvCDF(scene);
                        
                    #if INVERSEMAP_PSSMLT_DEBUG_SIMPLIFY_BDPT
                    // Always accept
                    ctx.currState.Swap(propState);
                    #else
                    // Scalar contributions
                    const auto currC = currPs.ScalarContrb();
                    const auto propC = propPs.ScalarContrb();

                    // MH update
                    const auto A = currC == 0_f ? 1_f : Math::Min(1_f, propC / currC);
                    if (ctx.rng.Next() < A)
                    {
                        ctx.currState.Swap(propState);
                    }
                    #endif
                }
                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Accumulate contribution
                {
                    const auto ps = ctx.currState.InvCDF(scene);
                    const auto I  = ps.ScalarContrb();
                    for (const auto& p : ps.ps)
                    {
                        const auto C = p.Cstar * p.w;
                        #if INVERSEMAP_PSSMLT_DEBUG_SIMPLIFY_BDPT
                        ctx.film->Splat(p.path.RasterPosition(), C);
                        #else
                        ctx.film->Splat(p.path.RasterPosition(), C * (b / I));
                        #endif
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

LM_COMPONENT_REGISTER_IMPL(Renderer_Invmap_PSSMLT, "renderer::invmap_pssmlt");

LM_NAMESPACE_END
