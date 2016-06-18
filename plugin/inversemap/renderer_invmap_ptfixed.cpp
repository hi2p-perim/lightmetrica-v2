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

LM_NAMESPACE_BEGIN

class Renderer_Invmap_PTFixed final : public Renderer
{
public:

    LM_IMPL_CLASS(Renderer_Invmap_PTFixed, Renderer);

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

    LM_IMPL_F(Render) = [this](const Scene* scene, Random* initRng, Film* film) -> void
    {
        //region Thread-specific context
        struct Context
        {
            Random rng;
            Film::UniquePtr film{ nullptr, nullptr };
        };
        std::vector<Context> contexts(Parallel::GetNumThreads());
        for (auto& ctx : contexts)
        {
            ctx.rng.SetSeed(initRng->NextUInt());
            ctx.film = ComponentFactory::Clone<Film>(film);
            ctx.film->Clear();
        }
        //endregion

        // --------------------------------------------------------------------------------

        Parallel::For(numMutations_, [&](long long index, int threadid, bool init) -> void
        {
            auto& ctx = contexts[threadid];

            // Generate primary sample
            std::vector<Float> primarySample;
            for (int i = 0; i < numVertices_; i++)
            {
                primarySample.push_back(ctx.rng.Next());
                primarySample.push_back(ctx.rng.Next());
            }

            // Map to path
            const auto path = InversemapUtils::MapPS2Path(scene, primarySample);
            if (path.vertices.size() == numVertices_)
            {
                // Record contribution
                const SPD F = path.EvaluateF(0);
                if (!F.Black())
                {
                    // Path probability
                    const auto p = path.EvaluatePathPDF(scene, 0);
                    assert(p > 0);

                    // Accumulate the contribution
                    const auto C = F / p;
                    const auto rasterPos = path.RasterPosition();
                    ctx.film->Splat(rasterPos, C);
                }
            }
        });

        // --------------------------------------------------------------------------------

        // Gather & Rescale
        film->Clear();
        for (auto& ctx : contexts)
        {
            film->Accumulate(ctx.film.get());
        }
        film->Rescale((Float)(film->Width() * film->Height()) / numMutations_);
    };

};

LM_COMPONENT_REGISTER_IMPL(Renderer_Invmap_PTFixed, "renderer::invmap::ptfixed");

LM_NAMESPACE_END
