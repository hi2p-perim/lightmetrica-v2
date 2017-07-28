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
#include "debugio.h"
#include <cereal/archives/json.hpp>
#include <cereal/types/vector.hpp>

#define INVERSEMAP_BDPT_DEBUG_IO 0

LM_NAMESPACE_BEGIN

class Renderer_Invmap_BDPT final : public Renderer
{
public:

    LM_IMPL_CLASS(Renderer_Invmap_BDPT, Renderer);

public:

    int maxNumVertices_;
    int minNumVertices_;
    long long numMutations_;
    double renderTime_;

public:

    LM_IMPL_F(Initialize) = [this](const PropertyNode* prop) -> bool
    {
        if (!prop->ChildAs<int>("max_num_vertices", maxNumVertices_)) return false;
        if (!prop->ChildAs<int>("min_num_vertices", minNumVertices_)) return false;
        numMutations_ = prop->ChildAs<long long>("num_mutations", 0);
        renderTime_ = prop->ChildAs<double>("render_time", -1);
        return true;
    };

    LM_IMPL_F(Render) = [this](const Scene* scene_, Random* initRng, const std::string& outputPath) -> void
    {
        #if INVERSEMAP_BDPT_DEBUG_IO
        DebugIO::Run();
        #endif

        // --------------------------------------------------------------------------------

        const auto* scene = static_cast<const Scene3*>(scene_);
        auto* film = static_cast<const Sensor*>(scene->GetSensor()->emitter)->GetFilm();

        // --------------------------------------------------------------------------------

        #if INVERSEMAP_BDPT_DEBUG_IO
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

        #pragma region Thread-specific context
        struct Context
        {
            Random rng;
            Film::UniquePtr film{ nullptr, nullptr };
            struct
            {
                Subpath subpathE;
                Subpath subpathL;
                Path fullpath;
            } cache;
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
        const auto processed = Parallel::For({ renderTime_ < 0 ? ParallelMode::Samples : ParallelMode::Time, numMutations_, renderTime_ }, [&](long long index, int threadid, bool init)
        {
            auto& ctx = contexts[threadid];

            // --------------------------------------------------------------------------------
            
            #pragma region Sample subpaths
            auto& subpathE = ctx.cache.subpathE;
            auto& subpathL = ctx.cache.subpathL;
            subpathE.vertices.clear();
            subpathL.vertices.clear();
            subpathE.SampleSubpathFromEndpoint(scene, &ctx.rng, TransportDirection::EL, maxNumVertices_);
            subpathL.SampleSubpathFromEndpoint(scene, &ctx.rng, TransportDirection::LE, maxNumVertices_);
            #pragma endregion

            // --------------------------------------------------------------------------------

            #pragma region Combine subpaths
            const int nE = (int)(subpathE.vertices.size());
            for (int t = 1; t <= nE; t++)
            {
                const int nL = (int)(subpathL.vertices.size());
                const int minS = Math::Max(0, Math::Max(2 - t, minNumVertices_ - t));
                const int maxS = Math::Min(nL, maxNumVertices_ - t);
                for (int s = minS; s <= maxS; s++)
                {
                    // Connect vertices and create a full path
                    auto& fullpath = ctx.cache.fullpath;
                    if (!fullpath.ConnectSubpaths(scene, subpathL, subpathE, s, t)) { continue; }

                    // Evaluate contribution
                    const auto Cstar = fullpath.EvaluateUnweightContribution(scene, s);
                    if (Cstar.Black())
                    {
                        continue;
                    }

                    // Evaluate MIS weight
                    const auto w = fullpath.EvaluateMISWeight(scene, s);

                    // Accumulate contribution
                    const auto C = Cstar * w;
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
        film->Rescale((Float)(film->Width() * film->Height()) / processed);
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

        #if INVERSEMAP_BDPT_DEBUG_IO
        DebugIO::Stop();
        #endif
    };

};

LM_COMPONENT_REGISTER_IMPL(Renderer_Invmap_BDPT, "renderer::invmap_bdpt");

LM_NAMESPACE_END
