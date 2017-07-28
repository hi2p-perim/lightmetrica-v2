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

#include "multiplexeddensity.h"

LM_NAMESPACE_BEGIN

class Renderer_Debug_MultiplexedDensity final : public Renderer
{
public:

    LM_IMPL_CLASS(Renderer_Debug_MultiplexedDensity, Renderer);

public:

    int numVertices_;
    long long numMutations_;

public:

    LM_IMPL_F(Initialize) = [this](const PropertyNode* prop) -> bool
    {
        if (!prop->ChildAs<int>("num_vertices", numVertices_)) return false;
        if (!prop->ChildAs<long long>("num_mutations", numMutations_)) return false;
        return true;
    };

    LM_IMPL_F(Render) = [this](const Scene* scene, Random* initRng, const std::string& outputPath) -> void
    {
        // Thread-specific context
        struct Context
        {
            Random rng;
            std::vector<std::vector<Float>> states;
            std::vector<std::vector<Float>> invStates;
        };
        std::vector<Context> contexts(Parallel::GetNumThreads());
        for (auto& ctx : contexts)
        {
            ctx.rng.SetSeed(initRng->NextUInt());
        }

        Parallel::For(numMutations_, [&](long long index, int threadid, bool init) -> void
        {
            auto& ctx = contexts[threadid];

            // Generate state
            MultiplexedDensity::State state(&ctx.rng, numVertices_);

            // Map to path space
            const auto path = MultiplexedDensity::InvCDF(state, scene);
            if (!path)
            {
                return;
            }

            // Remap to multiplexed primary sample space
            const auto invS = MultiplexedDensity::CDF(path->path, path->s, scene, &ctx.rng);
            if (!invS)
            {
                return;
            }

            // Record states
            ctx.states.push_back(state.ToVector());
            ctx.invStates.push_back(invS->ToVector());
        });

    };

};

LM_COMPONENT_REGISTER_IMPL(Renderer_Invmap_MMLTInvmapFixed, "renderer::invmap_mmltinvmapfixed");

LM_NAMESPACE_END
