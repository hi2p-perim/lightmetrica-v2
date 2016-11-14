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

LM_NAMESPACE_BEGIN

// --------------------------------------------------------------------------------

enum class MMLTInvmapFixed_Strategy : int
{
    // Path space mutations
    Bidir      = (int)(MLTStrategy::Bidir),
    Lens       = (int)(MLTStrategy::Lens),
    Caustic    = (int)(MLTStrategy::Caustic),
    Multichain = (int)(MLTStrategy::Multichain),
    Identity   = (int)(MLTStrategy::Identity),

    // Primary sample space mutations
    SmallStep,
    LargeStep,
};

// --------------------------------------------------------------------------------

namespace MultiplexedDensity
{

struct State
{

    int numVertices_;
    Float uT_;                  // For technique selection
    std::vector<Float> usL_;    // For light subpath
    std::vector<Float> usE_;    // For eye subpath

public:

    State() {}

    State(Random* rng, int numVertices)
        : numVertices_(numVertices)
    {
        // Consumes 3 random numbers for sampling a vertex
        const auto numStates = numVertices * 3;
        usL_.assign(numStates, 0_f);
        usE_.assign(numStates, 0_f);
        uT_ = rng->Next();
        for (auto& u : usE_) u = rng->Next();
        for (auto& u : usL_) u = rng->Next();
    }

    State(const State& o)
        : numVertices_(o.numVertices_)
        , uT_(o.uT_)
        , usL_(o.usL_)
        , usE_(o.usE_)
    {}

public:

    auto Swap(State& o) -> void
    {
        std::swap(uT_, o.uT_);
        usL_.swap(o.usL_);
        usE_.swap(o.usE_);
    }

public:

    // Large step mutation
    auto LargeStep(Random* rng) const -> State
    {
        State next(*this);
        next.uT_ = rng->Next();
        for (auto& u : next.usE_) u = rng->Next();
        for (auto& u : next.usL_) u = rng->Next();
        return next;
    }

    // Small step mutation
    auto SmallStep(Random* rng) const -> State
    {
        const auto Perturb = [](Random* rng, const Float u, const Float s1, const Float s2)
        {
            Float result;
            Float r = rng->Next();
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

        State next(*this);
        next.uT_ = Perturb(rng, uT_, s1, s2);
        for (size_t i = 0; i < usE_.size(); i++) next.usE_[i] = Perturb(rng, usE_[i], s1, s2);
        for (size_t i = 0; i < usL_.size(); i++) next.usL_[i] = Perturb(rng, usL_[i], s1, s2);

        return next;
    }

};

struct CachedPath
{
    int s, t;
    Path path;
    SPD Cstar;      // Caches unweighed contribution
    Float w;        // Caches MIS weight
};
auto InvCDF(const State& s, const Scene* scene) -> boost::optional<CachedPath>
{
    Subpath subpathE;
    Subpath subpathL;
    subpathE.SampleSubpathWithPrimarySamples(scene, s.usE_, TransportDirection::EL, s.numVertices_);
    subpathL.SampleSubpathWithPrimarySamples(scene, s.usL_, TransportDirection::LE, s.numVertices_);

    CachedPath p;
    p.t = Math::Min(s.numVertices_, (int)(s.uT_ * (s.numVertices_ + 1)));
    p.s = s.numVertices_ - p.t;

    const int nE = (int)(subpathE.vertices.size());
    const int nL = (int)(subpathL.vertices.size());
    if (p.t > nE || p.s > nL) { return boost::none; }

    if (!p.path.ConnectSubpaths(scene, subpathL, subpathE, p.s, p.t)) { return boost::none; }
    p.Cstar = p.path.EvaluateUnweightContribution(scene, p.s);
    if (p.Cstar.Black()) { return boost::none; }

    p.w = p.path.EvaluateMISWeight(scene, p.s);
    return p;
}

// Maps a path to a state in multiplexed primary sample space
auto CDF(const Path& p, int s, const Scene* scene, Random* rng) -> boost::optional<State>
{
    const int n = (int)(p.vertices.size());
    const int t = n - s;

    // This ensures uninitialized parts is filled with fresh random numbers
    State state(rng, n);
    
    // Map subpaths
    const auto usL = CDF_Subpath(p, s, rng, TransportDirection::LE);
    assert(usL.size() <= state.usL_.size());
    for (size_t i = 0; i < usL.size(); i++) { state.usL_[i] = usL[i]; }
    const auto usE = CDF_Subpath(p, t, rng, TransportDirection::EL);
    assert(usE.size() <= state.usE_.size());
    for (size_t i = 0; i < usE.size(); i++) { state.usE_[i] = usE[i]; }
    
    // Map technique
    state.uT_ = Math::Clamp((Float)t + rng->Next() / (n + 1), 0_f, 1_f);
    
    return state;
}

auto CDF_Subpath(const Path& p, int k, Random* rng, TransportDirection transDir) -> std::vector<Float>
{
    const int n = (int)(p.vertices.size());
    const auto index = [&](int i) { return transDir == TransportDirection::LE ? i : n - 1 - i; };

    std::vector<Float> us;
    for (int i = 0; i < k; i++)
    {
        const auto* v  = &p.vertices[index(i)];
        const auto* vp = index(i - 1) >= 0 && index(i - 1) < n ? &p.vertices[index(i - 1)] : nullptr;
        const auto* vn = index(i + 1) >= 0 && index(i + 1) < n ? &p.vertices[index(i + 1)] : nullptr;
        assert(vp != nullptr || nv != nullptr);
        
        if (i == 0)
        {
            if (transDir == TransportDirection::EL)
            {
                // Pinhole camera
                assert(std::strcmp(v->primitive->sensor->implName, "Sensor_Pinhole") == 0);
                us.push_back(rng->Next());
                us.push_back(rng->Next());
                us.push_back(rng->Next());
            }
            else
            {
                // Area light
                assert(std::strcmp(v->primitive->emitter->implName, "Light_Area") == 0);
                const auto* triAreaDist = v->primitive->light->TriAreaDist();
                const auto u = InversemapUtils::SampleTriangleMesh_Inverse(v->primitive, *triAreaDist, v->geom);
                us.push_back(u[0]);
                us.push_back(u[1]);
                us.push_back(rng->Next());
            }
        }
        else
        {
            LM_TBA();
        }
    }

}

}

// --------------------------------------------------------------------------------

///! MMLT with fused mutation (fixed path length).
class Renderer_Invmap_MMLTInvmapFixed final : public Renderer
{
public:

    LM_IMPL_CLASS(Renderer_Invmap_MMLTInvmapFixed, Renderer);

public:

    int numVertices_;
    long long numMutations_;
    std::vector<Float> strategyWeights_{ 1_f, 1_f, 1_f, 1_f, 1_f, 1_f, 1_f };
    #if INVERSEMAP_OMIT_NORMALIZATION
    Float normalization_;
    #endif

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
            strategyWeights_[(int)(MMLTInvmapFixed_Strategy::Bidir)]      = child->ChildAs<Float>("bidir", 1_f);
            strategyWeights_[(int)(MMLTInvmapFixed_Strategy::Lens)]       = child->ChildAs<Float>("lens", 1_f);
            strategyWeights_[(int)(MMLTInvmapFixed_Strategy::Caustic)]    = child->ChildAs<Float>("caustic", 1_f);
            strategyWeights_[(int)(MMLTInvmapFixed_Strategy::Multichain)] = child->ChildAs<Float>("multichain", 1_f);
            strategyWeights_[(int)(MMLTInvmapFixed_Strategy::Identity)]   = child->ChildAs<Float>("identity", 0_f);
            strategyWeights_[(int)(MMLTInvmapFixed_Strategy::SmallStep)]  = child->ChildAs<Float>("smallstep", 1_f);
            strategyWeights_[(int)(MMLTInvmapFixed_Strategy::LargeStep)]  = child->ChildAs<Float>("largestep", 1_f);
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

                    // Sanity check
                    const auto invS = MultiplexedDensity::CDF(path->path, path->s, numVertices_, scene);
                    if (!invS)
                    {
                        continue;
                    }
                    const auto path_invS = MultiplexedDensity::InvCDF(*invS, scene);
                    if (!path_invS)
                    {
                        continue;
                    }
                    const auto C1 = (path->Cstar * path->w).Luminance();
                    const auto C2 = (path_invS->Cstar * path_invS->w).Luminance();
                    if (path->s != path_invS->s || path->t != path_invS->t || Math::Abs(C1 - C2) > Math::Eps())
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
                        static thread_local const auto StrategyDist = [&]() -> Distribution1D
                        {
                            Distribution1D dist;
                            for (Float w : strategyWeights_) dist.Add(w);
                            dist.Normalize();
                            return dist;
                        }();
                        return (MMLTInvmapFixed_Strategy)(StrategyDist.Sample(ctx.rng.Next()));
                    }();
                    #pragma endregion

                    // --------------------------------------------------------------------------------

                    if (strategy == MMLTInvmapFixed_Strategy::SmallStep || strategy == MMLTInvmapFixed_Strategy::LargeStep)
                    {
                        #pragma region Primary sample space mutations

                        // Mutate
                        auto prop = strategy == MMLTInvmapFixed_Strategy::LargeStep ? ctx.curr.LargeStep(&ctx.rng) : ctx.curr.SmallStep(&ctx.rng);

                        // Paths
                        const auto currP = MultiplexedDensity::InvCDF(ctx.curr, scene);
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
                            const auto Qxy = MLTMutationStrategy::Q((MLTStrategy)(strategy), scene, currP.path, propP->p, propP->kd, propP->dL);
                            const auto Qyx = MLTMutationStrategy::Q((MLTStrategy)(strategy), scene, propP->p, currP.path, propP->kd, propP->dL);
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
                                // Map to primary sample space
                                const auto propInvS = MultiplexedDensity::CDF(propP->p, currP.s, numVertices_, scene);
                                if (!propInvS)
                                {
                                    return false;
                                }

                                // Sanity check
                                const auto path_propInvS = MultiplexedDensity::InvCDF(*propInvS, scene);
                                const auto C1 = (currP.Cstar * currP.w).Luminance();
                                const auto C2 = (path_propInvS->Cstar * path_propInvS->w).Luminance();
                                if (currP.s != path_propInvS->s || currP.t != path_propInvS->t || Math::Abs(C1 - C2) > Math::Eps())
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
                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Accumulate contribution
                {
                    const auto p = MultiplexedDensity::InvCDF(ctx.curr, scene);
                    assert(p);
                    const auto C = p->Cstar * p->w;
                    const auto I = InversemapUtils::ScalarContrb(C);
                    ctx.film->Splat(p->path.RasterPosition(), C * (b / I));
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
