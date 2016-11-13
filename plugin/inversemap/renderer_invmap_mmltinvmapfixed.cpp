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

enum class MMLTInvmapFixed_Strategy : int
{
    // Path space mutations
    Bidir      = (int)(Strategy::Bidir),
    Lens       = (int)(Strategy::Lens),
    Caustic    = (int)(Strategy::Caustic),
    Multichain = (int)(Strategy::Multichain),
    Identity   = (int)(Strategy::Identity),

    // Primary sample space mutations
    SmallStep,
    LargeStep,
};

namespace MultiplexedDensity
{

class State
{
private:

    int numVertices_;
    Float uT_;                  // For technique selection
    std::vector<Float> usL_;    // For light subpath
    std::vector<Float> usE_;    // For eye subpath

public:

    State() {}

    State(Random* rng, int numVertices)
        : numVertices_(numVertices)
    {
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
    LM_TBA
}

auto CDF(const Path& p, const Scene* scene) -> boost::optional<State>
{
    LM_TBA
}

}

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
                    const auto invS = MultiplexedDensity::CDF(path->path, scene);
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

                }();
                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Accumulate contribution
                {
                    
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

LM_COMPONENT_REGISTER_IMPL(Renderer_Invmap_MMLTInvmapFixed, "renderer::invmap_mmltinvmapfixed");

LM_NAMESPACE_END
