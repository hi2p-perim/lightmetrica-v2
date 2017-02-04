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
#include "manifoldutils.h"
#include "debugio.h"
#include <chrono>
#include <thread>
#include <cereal/archives/json.hpp>
#include <cereal/types/vector.hpp>

#define INVERSEMAP_MANIFOLDWALK_OUTPUT_TRIANGLES          0
#define INVERSEMAP_MANIFOLDWALK_OUTPUT_FAILED_TRIAL_PATHS 0
#define INVERSEMAP_MANIFOLDWALK_SINGLE_TARGET             1
#define INVERSEMAP_MANIFOLDWALK_CONSTRAINT_CONSISTENCY    0

LM_NAMESPACE_BEGIN

class Renderer_Debug_ManifoldWalk final : public Renderer
{
public:

    LM_IMPL_CLASS(Renderer_Debug_ManifoldWalk, Renderer);

public:

    LM_IMPL_F(Initialize) = [this](const PropertyNode* prop) -> bool
    {
        return true;
    };

    LM_IMPL_F(Render) = [this](const Scene* scene, Random* initRng, const std::string& outputPath) -> void
    {
        #if INVERSEMAP_MANIFOLDWALK_DEBUG_IO
        DebugIO::Run();
        #endif

        // --------------------------------------------------------------------------------

        #if INVERSEMAP_MANIFOLDWALK_OUTPUT_TRIANGLES
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

        #if INVERSEMAP_MANIFOLDWALK_DEBUG_IO
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
        }
        #endif


        // --------------------------------------------------------------------------------

        // Sample a light subpath 
        Subpath subpathL;
        {
            // 1
            {
                SubpathSampler::PathVertex v;
                v.type = SurfaceInteractionType::L;
                v.primitive = scene->SampleEmitter(v.type, 0_f);

                Vec3 _;
                v.primitive->SamplePositionAndDirection(Vec2(), Vec2(), v.geom, _);
                v.geom.p.x = 0_f;
                v.geom.p.z = 0_f;

                subpathL.vertices.push_back(v);
            }
            
            // 2
            {
                const auto& pv = subpathL.vertices.back();
                Ray ray = { pv.geom.p, Vec3(0_f, -1_f, 0_f) };
                Intersection isect;
                if (!scene->Intersect(ray, isect)) { LM_UNREACHABLE(); return; }

                SubpathSampler::PathVertex v;
                v.geom = isect.geom;
                v.primitive = isect.primitive;
                v.type = isect.primitive->Type() & ~SurfaceInteractionType::Emitter;
                subpathL.vertices.push_back(v);
            }

            // 3
            for (int i = 0; i < 2; i++)
            {
                const auto& pv = subpathL.vertices.back();
                const auto& ppv = subpathL.vertices[subpathL.vertices.size()-2];

                Ray ray;
                ray.o = pv.geom.p;
                pv.primitive->SampleDirection(Vec2(), 1_f, pv.type, pv.geom, Math::Normalize(ppv.geom.p - pv.geom.p), ray.d);
                Intersection isect;
                if (!scene->Intersect(ray, isect)) { LM_UNREACHABLE(); return; }

                SubpathSampler::PathVertex v;
                v.geom = isect.geom;
                v.primitive = isect.primitive;
                v.type = isect.primitive->Type() & ~SurfaceInteractionType::Emitter;
                subpathL.vertices.push_back(v);
            }
        }

        // --------------------------------------------------------------------------------

        #if INVERSEMAP_MANIFOLDWALK_OUTPUT_FAILED_TRIAL_PATHS
        {
            static long long count = 0;
            if (count == 0)
            {
                boost::filesystem::remove("dirs_orig.out");
            }
            {
                count++;
                std::ofstream out("dirs_orig.out", std::ios::out | std::ios::app);
                for (const auto& v : subpathL.vertices)
                {
                    out << boost::str(boost::format("%.10f %.10f %.10f ") % v.geom.p.x % v.geom.p.y % v.geom.p.z);
                }
                out << std::endl;
            }
        }
        #endif

        // --------------------------------------------------------------------------------

        // For each points on intersected quad
        const int BinSize = 100;
        std::vector<Float> dist(BinSize*BinSize);
        #if INVERSEMAP_MANIFOLDWALK_SINGLE_TARGET

        #if INVERSEMAP_MANIFOLDWALK_DEBUG_IO
        int I;
        LM_LOG_DEBUG("waiting_for_input");
        {
            DebugIO::Wait();
            std::stringstream ss(DebugIO::Input());
            {
                cereal::JSONInputArchive ia(ss);
                ia(cereal::make_nvp("selected_target_id", I));
            }
        }
        #else
        const int I = 44;
        #endif
        
        for (int i = I; i <= I; i++)
        #else
        for (int i = 0; i < BinSize; i++)
        #endif
        {
            #if INVERSEMAP_MANIFOLDWALK_SINGLE_TARGET
            for (int j = I; j <= I; j++)
            #else
            for (int j = 0; j < BinSize; j++)
            #endif
            {
                //const auto D = 1_f / BinSize;
                Vec3 p;
                p.x = (((Float)j + 0.5_f) / BinSize) * 2_f - 1_f;
                p.y = -1_f;
                p.z = (((Float)i + 0.5_f) / BinSize) * 2_f - 1_f;

                #if INVERSEMAP_MANIFOLDWALK_OUTPUT_FAILED_TRIAL_PATHS
                {
                    static long long count = 0;
                    if (count == 0)
                    {
                        boost::filesystem::remove("targets.out");
                    }
                    {
                        count++;
                        std::ofstream out("targets.out", std::ios::out | std::ios::app);
                        out << boost::str(boost::format("%.10f %.10f %.10f ") % p.x % p.y % p.z);
                        out << std::endl;
                    }
                }
                #endif

                // Run ManifoldWalk for the new point p
                const auto connPath = ManifoldUtils::WalkManifold(scene, subpathL, p);
                if (!connPath)
                {
                    dist[i*BinSize + j] = 0_f;
                    continue;
                }

                const auto connPathInv = ManifoldUtils::WalkManifold(scene, *connPath, subpathL.vertices.back().geom.p);
                if (!connPathInv)
                {
                    dist[i*BinSize + j] = 0.5_f;
                    continue;
                }

                dist[i*BinSize + j] = 1_f;
            }
        }

        // --------------------------------------------------------------------------------

        // Record data
        {
            const auto path = outputPath + ".dat";
            LM_LOG_INFO("Saving output: " + path);
            const auto parent = boost::filesystem::path(path).parent_path();
            if (!boost::filesystem::exists(parent) && parent != "") { boost::filesystem::create_directories(parent); }
            std::ofstream ofs(path, std::ios::out | std::ios::binary);
            // Bin size
            ofs.write((const char*)&BinSize, sizeof(int));
            ofs.write((const char*)dist.data(), sizeof(Float) * BinSize * BinSize);
        }

        // --------------------------------------------------------------------------------

        #if INVERSEMAP_MANIFOLDWALK_DEBUG_IO
        DebugIO::Stop();
        #endif
    };

};

LM_COMPONENT_REGISTER_IMPL(Renderer_Debug_ManifoldWalk, "renderer::invmap_debug_manifoldwalk");

LM_NAMESPACE_END
