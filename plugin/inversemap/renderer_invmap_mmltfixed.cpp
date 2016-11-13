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

#define INVERSEMAP_MMLTFIXED_DEBUG_PRINT_AVE_ACC 1
#define INVERSEMAP_MMLTFIXED_DEBUG_SIMPLIFY_STRATEGY_PT 0
#define INVERSEMAP_MMLTFIXED_DEBUG_SIMPLIFY_ALWAYS_ACCEPT 0
#if INVERSEMAP_MMLTFIXED_DEBUG_SIMPLIFY_ALWAYS_ACCEPT && !INVERSEMAP_MMLTFIXED_DEBUG_SIMPLIFY_STRATEGY_PT
#error "Invalid combination"
#endif

LM_NAMESPACE_BEGIN

class MMLTFixedState
{
private:

    int numVertices_;
    Float uT_;                  // For technique selection
    std::vector<Float> usL_;    // For light subpath
    std::vector<Float> usE_;    // For eye subpath

public:

    MMLTFixedState() {}

    MMLTFixedState(Random* rng, int numVertices)
        : numVertices_(numVertices)
    {
        const auto numStates = numVertices * 3;
        usL_.assign(numStates, 0_f);
        usE_.assign(numStates, 0_f);
        uT_ = rng->Next();
        for (auto& u : usE_) u = rng->Next();
        for (auto& u : usL_) u = rng->Next();
    }

    MMLTFixedState(const MMLTFixedState& o)
        : numVertices_(o.numVertices_)
        , uT_(o.uT_)
        , usL_(o.usL_)
        , usE_(o.usE_)
    {}

public:

    auto Swap(MMLTFixedState& o) -> void
    {
        std::swap(uT_, o.uT_);
        usL_.swap(o.usL_);
        usE_.swap(o.usE_);
    }

public:

    // Large step mutation
    auto LargeStep(Random* rng) const -> MMLTFixedState
    {
        MMLTFixedState next(*this);
        next.uT_ = rng->Next();
        for (auto& u : next.usE_) u = rng->Next();
        for (auto& u : next.usL_) u = rng->Next();
        return next;
    }

    // Small step mutation
    auto SmallStep(Random* rng) const -> MMLTFixedState
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

        MMLTFixedState next(*this);
        next.uT_ = Perturb(rng, uT_, s1, s2);
        for (size_t i = 0; i < usE_.size(); i++) next.usE_[i] = Perturb(rng, usE_[i], s1, s2);
        for (size_t i = 0; i < usL_.size(); i++) next.usL_[i] = Perturb(rng, usL_[i], s1, s2);

        return next;
    }

public:

    // Sample a path using current state
    struct CachedPath
    {
        int s, t;
        Path path;
        SPD Cstar;      // Caches unweighed contribution
        Float w;        // Caches MIS weight
        auto ScalarContrb() const -> Float
        {
            return InversemapUtils::ScalarContrb(Cstar * w);
        }
    };
    auto InvCDF(const Scene* scene) const -> boost::optional<CachedPath>
    {
        Subpath subpathE;
        Subpath subpathL;
        subpathE.SampleSubpathWithPrimarySamples(scene, usE_, TransportDirection::EL, numVertices_);
        subpathL.SampleSubpathWithPrimarySamples(scene, usL_, TransportDirection::LE, numVertices_);

        CachedPath p;
        #if INVERSEMAP_MMLTFIXED_DEBUG_SIMPLIFY_STRATEGY_PT
        p.t = numVertices_;
        p.s = 0;
        #else
        p.t = Math::Min(numVertices_, (int)(uT_ * (numVertices_ + 1)));
        p.s = numVertices_ - p.t;
        #endif

        const int nE = (int)(subpathE.vertices.size());
        const int nL = (int)(subpathL.vertices.size());
        if (p.t > nE || p.s > nL) { return boost::none; }

        if (!p.path.ConnectSubpaths(scene, subpathL, subpathE, p.s, p.t)) { return boost::none; }
        p.Cstar = p.path.EvaluateUnweightContribution(scene, p.s);
        if (p.Cstar.Black()) { return boost::none; }

        p.w = p.path.EvaluateMISWeight(scene, p.s);
        return p;
    }

};

///! Multiplexed metropolis light transport (fixed path length version).
class Renderer_Invmap_MMLTFixed final : public Renderer
{
public:

    LM_IMPL_CLASS(Renderer_Invmap_MMLTFixed, Renderer);

public:

    int numVertices_;
    long long numMutations_;
    Float largeStepProb_;
    #if INVERSEMAP_OMIT_NORMALIZATION
    Float normalization_;
    #endif

public:

    LM_IMPL_F(Initialize) = [this](const PropertyNode* prop) -> bool
    {
        if (!prop->ChildAs<int>("num_vertices", numVertices_)) return false;
        if (!prop->ChildAs<long long>("num_mutations", numMutations_)) return false;
        largeStepProb_ = prop->ChildAs<Float>("large_step_prob", 0.5_f);
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
                MMLTFixedState curr;
                #if INVERSEMAP_MMLTFIXED_DEBUG_PRINT_AVE_ACC
                long long acceptCount = 0;
                #endif
            };
            std::vector<Context> contexts(Parallel::GetNumThreads());
            for (auto& ctx : contexts)
            {
                ctx.rng.SetSeed(initRng->NextUInt());
                ctx.film = ComponentFactory::Clone<Film>(film);

                // Initial state
                while (true)
                {
                    MMLTFixedState state(initRng, numVertices_);
                    const auto path = state.InvCDF(scene);
                    if (!path)
                    {
                        continue;
                    }

                    ctx.curr = std::move(state);
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
                    // Mutate
                    auto prop = ctx.rng.Next() < largeStepProb_
                        ? ctx.curr.LargeStep(&ctx.rng)
                        : ctx.curr.SmallStep(&ctx.rng);

                    // Paths
                    const auto currP = ctx.curr.InvCDF(scene);
                    const auto propP = prop.InvCDF(scene);
                    if (!propP)
                    {
                        return false;
                    }

                    #if INVERSEMAP_MMLTFIXED_DEBUG_SIMPLIFY_ALWAYS_ACCEPT
                    ctx.curr.Swap(prop);
                    #else
                    // Scalar contributions
                    #if INVERSEMAP_MMLTFIXED_DEBUG_SIMPLIFY_STRATEGY_PT
                    const auto currC = InversemapUtils::ScalarContrb(currP->Cstar);
                    const auto propC = InversemapUtils::ScalarContrb(propP->Cstar);
                    #else
                    const auto currC = currP->ScalarContrb();
                    const auto propC = propP->ScalarContrb();
                    #endif

                    // MH update
                    const auto A = currC == 0_f ? 1_f : Math::Min(1_f, propC / currC);
                    if (ctx.rng.Next() < A)
                    {
                        ctx.curr.Swap(prop);
                        return true;
                    }

                    return false;
                    #endif
                }();
                #pragma endregion

                // --------------------------------------------------------------------------------

                #if INVERSEMAP_MMLTFIXED_DEBUG_PRINT_AVE_ACC
                if (accept) { ctx.acceptCount++; }
                #else
                LM_UNUSED(accept);
                #endif

                // --------------------------------------------------------------------------------

                #pragma region Accumulate contribution
                {
                    const auto p = ctx.curr.InvCDF(scene);
                    assert(p);
                    #if INVERSEMAP_MMLTFIXED_DEBUG_SIMPLIFY_ALWAYS_ACCEPT
                    ctx.film->Splat(p->path.RasterPosition(), p->Cstar);
                    #elif INVERSEMAP_MMLTFIXED_DEBUG_SIMPLIFY_STRATEGY_PT
                    const auto currF = p->path.EvaluateF(0);
                    assert(!currF.Black());
                    ctx.film->Splat(p->path.RasterPosition(), currF * (b / InversemapUtils::ScalarContrb(currF)));
                    #else
                    const auto I = p->ScalarContrb();
                    const auto C = p->Cstar * p->w;
                    ctx.film->Splat(p->path.RasterPosition(), C * (b / I));
                    #endif
                }
                #pragma endregion
            });

            // --------------------------------------------------------------------------------

            #if INVERSEMAP_MMLTFIXED_DEBUG_PRINT_AVE_ACC
            {
                long long sum = 0;
                for (auto& ctx : contexts) { sum += ctx.acceptCount; }
                const double ave = (double)sum / numMutations_;
                LM_LOG_INFO(boost::str(boost::format("Ave. acceptance ratio: %.5f (%d / %d)") % ave % sum % numMutations_));
            }
            #endif

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

LM_COMPONENT_REGISTER_IMPL(Renderer_Invmap_MMLTFixed, "renderer::invmap_mmltfixed");

LM_NAMESPACE_END
