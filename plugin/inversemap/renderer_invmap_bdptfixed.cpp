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

class Renderer_Invmap_BDPTFixed final : public Renderer
{
public:

    LM_IMPL_CLASS(Renderer_Invmap_BDPTFixed, Renderer);

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
        #pragma region Thread-specific context
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
        #pragma endregion

        // --------------------------------------------------------------------------------

        #pragma region Parallel loop
        Parallel::For(numMutations_, [&](long long index, int threadid, bool init)
        {
            auto& ctx = contexts[threadid];

            // --------------------------------------------------------------------------------

            #pragma region Sample subpaths
            Subpath subpathE;
            Subpath subpathL;
            subpathE.SampleSubpath(scene, &ctx.rng, TransportDirection::EL, numVertices_);
            subpathL.SampleSubpath(scene, &ctx.rng, TransportDirection::LE, numVertices_);
            #pragma endregion

            // --------------------------------------------------------------------------------

            #pragma region Combine subpaths
            const int nE = (int)(subpathE.vertices.size());
            for (int t = 1; t <= nE; t++)
            {
                const int nL = (int)(subpathL.vertices.size());
                const int minS = Math::Max(0, Math::Max(2 - t, numVertices_ - t));
                const int maxS = Math::Min(nL, numVertices_ - t);
                for (int s = minS; s <= maxS; s++)
                {
                    if (s + t != numVertices_) { continue; }

                    // Connect vertices and create a full path
                    Path fullpath;
                    if (!fullpath.ConnectSubpaths(scene, subpathL, subpathE, s, t)) { continue; }

                    // Evaluate contribution
                    const auto f = fullpath.EvaluateF(s);
                    if (f.Black()) { continue; }

                    // Evaluate connection PDF
                    const auto p = fullpath.EvaluatePathPDF(scene, s);
                    if (p.v == 0)
                    {
                        // Due to precision issue, this can happen.
                        return;
                    }

                    // Evaluate MIS weight
                    const auto w = fullpath.EvaluateMISWeight(scene, s);

                    // Accumulate contribution
                    const auto C = f * w / p;
                    ctx.film->Splat(fullpath.RasterPosition(), C);
                }
            }
            #pragma endregion
        });
        #pragma endregion

        // --------------------------------------------------------------------------------
        
        #pragma region Gather & Rescale
        film->Clear();
        for (auto& ctx : contexts)
        {
            film->Accumulate(ctx.film.get());
        }
        film->Rescale((Float)(film->Width() * film->Height()) / numMutations_);
        #pragma endregion
    };

};

LM_COMPONENT_REGISTER_IMPL(Renderer_Invmap_BDPTFixed, "renderer::invmap_bdptfixed");

LM_NAMESPACE_END
