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
#include <fstream>
#include <boost/format.hpp>
#include <boost/filesystem.hpp>

#define INVERSEMAP_PTFIXED_DEBUG 0

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
        #if INVERSEMAP_PTFIXED_DEBUG
        // Output triangles
        {
            std::ofstream out("tris.out", std::ios::out | std::ios::trunc);
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
                    out << p1.x << " " << p1.y << " " << p1.z << " "
                        << p2.x << " " << p2.y << " " << p2.z << " "
                        << p3.x << " " << p3.y << " " << p3.z << " " 
                        << p1.x << " " << p1.y << " " << p1.z << std::endl;
                }
            }
        }
        #endif

        // --------------------------------------------------------------------------------

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

        Parallel::For(numMutations_, [&](long long index, int threadid, bool init) -> void
        {
            auto& ctx = contexts[threadid];

            // Generate primary sample
            std::vector<Float> primarySample;
            for (int i = 0; i < InversemapUtils::NumSamples(numVertices_); i++)
            {
                primarySample.push_back(ctx.rng.Next());
            }

            // Map to path
            const auto path = InversemapUtils::MapPS2Path(scene, primarySample);
            if (!path || path->vertices.size() != numVertices_)
            {
                return;
            }

            // Record contribution
            const SPD F = path->EvaluateF(0);
            if (!F.Black())
            {
                // Path probability
                const auto p = path->EvaluatePathPDF(scene, 0);
                assert(p > 0);

                // Accumulate the contribution
                const auto C = F / p;
                const auto rasterPos = path->RasterPosition();
                ctx.film->Splat(rasterPos, C);

                // Check inverse mapping
                #if 0
                const auto ps = InversemapUtils::MapPath2PS(*path);
                for (int i = 0; i < InversemapUtils::NumSamples(numVertices_); i++)
                {
                    if (Math::Abs(ps[i] - primarySample[i]) > Math::EpsLarge())
                    {
                        __debugbreak();
                    }
                }
                #endif
            }

            #if INVERSEMAP_PTFIXED_DEBUG
            // Output sampled path
            static long long count = 0;
            if (count == 0)
            {
                boost::filesystem::remove("dirs.out");
            }
            if (count < 100)
            {
                count++;
                std::ofstream out("dirs.out", std::ios::out | std::ios::app);
                for (const auto& v : path.vertices)
                {
                    out << boost::str(boost::format("%.10f %.10f %.10f ") % v.geom.p.x % v.geom.p.y % v.geom.p.z);
                }
                out << std::endl;
            }
            #endif
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

LM_COMPONENT_REGISTER_IMPL(Renderer_Invmap_PTFixed, "renderer::invmap_ptfixed");

LM_NAMESPACE_END
