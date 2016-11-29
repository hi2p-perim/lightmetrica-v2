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
#include "manifoldutils.h"
#include "debugio.h"
#include <cereal/archives/json.hpp>
#include <cereal/types/vector.hpp>

#if LM_DEBUG_MODE
#define LM_PT_MANIFOLDNEE_DEBUG_IO 0
#else
#define LM_PT_MANIFOLDNEE_DEBUG_IO 0
#endif

LM_NAMESPACE_BEGIN

class Renderer_PT_ManifoldNEE final : public Renderer
{
public:

    LM_IMPL_CLASS(Renderer_PT_ManifoldNEE, Renderer);

public:

    Renderer_PT_ManifoldNEE()
        : sched_(ComponentFactory::Create<Scheduler>())
    {}

public:

    LM_IMPL_F(Initialize) = [this](const PropertyNode* prop) -> bool
    {
        sched_->Load(prop);
        maxNumVertices_ = prop->ChildAs<int>("max_num_vertices", -1);
        return true;
    };

    LM_IMPL_F(Render) = [this](const Scene* scene, Random* initRng, const std::string& outputPath) -> void
    {
        #if LM_PT_MANIFOLDNEE_DEBUG_IO
        DebugIO::Run();
        #endif

        // --------------------------------------------------------------------------------

        #if LM_PT_MANIFOLDNEE_DEBUG_IO
        {
            LM_LOG_DEBUG("triangle_vertices");
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

        auto* film_ = static_cast<const Sensor*>(scene->GetSensor()->emitter)->GetFilm();
        sched_->Process(scene, film_, initRng, [&](Film* film, Random* rng)
        {
            #pragma region Sample a sensor

            const auto* E = scene->SampleEmitter(SurfaceInteractionType::E, rng->Next());
            const auto pdfE = scene->EvaluateEmitterPDF(E);
            assert(pdfE.v > 0);

            #pragma endregion

            // --------------------------------------------------------------------------------

            #pragma region Sample a position on the sensor and initial ray direction

            SurfaceGeometry geomE;
            Vec3 initWo;
            E->sensor->SamplePositionAndDirection(rng->Next2D(), rng->Next2D(), geomE, initWo);
            const auto pdfPE = E->sensor->EvaluatePositionGivenDirectionPDF(geomE, initWo, false);
            assert(pdfPE.v > 0);

            #pragma endregion

            // --------------------------------------------------------------------------------

            #pragma region Temporary variables

            auto throughput = E->sensor->EvaluatePosition(geomE, false) / pdfPE / pdfE;
            const auto* primitive = E;
            int type = SurfaceInteractionType::E;
            auto geom = geomE;
            Vec3 wi;
            Vec2 rasterPos;
            int numVertices = 1;
            int lastNonSIndex = 0;

            #pragma endregion

            // --------------------------------------------------------------------------------

            while (true)
            {
                if (maxNumVertices_ != -1 && numVertices >= maxNumVertices_)
                {
                    break;
                }

                // --------------------------------------------------------------------------------

                #pragma region Direct light sampling
                [&]()
                {
                    if ((type & SurfaceInteractionType::S) > 0)
                    {
                        return;
                    }

                    // --------------------------------------------------------------------------------

                    // Sample seed subpath
                    const auto subpathL = [&]() -> boost::optional<Subpath>
                    {
                        Subpath subpathL;
                        for (int i = 0; i < maxNumVertices_ - numVertices + 1; i++)
                        {
                            if (i == 0)
                            {
                                // Initial vertex
                                Vec3 _;
                                SubpathSampler::PathVertex v;
                                v.type = SurfaceInteractionType::L;
                                v.primitive = scene->SampleEmitter(v.type, rng->Next());
                                v.primitive->SamplePositionAndDirection(rng->Next2D(), rng->Next2D(), v.geom, _);
                                subpathL.vertices.push_back(v);
                            }
                            else
                            {
                                // Vertices
                                const auto* pv  = &subpathL.vertices[i - 1];
                                const auto* ppv = i - 2 >= 0 ? &subpathL.vertices[i - 2] : nullptr;

                                // Directions
                                const auto wi = ppv ? Math::Normalize(ppv->geom.p - pv->geom.p) : Vec3();
                                const auto wo = [&]() -> Vec3
                                {
                                    if (i == 1)
                                    {
                                        // Direction from the position on light and orignal last vertex
                                        return Math::Normalize(geom.p - pv->geom.p);
                                    }
                                    else
                                    {
                                        Vec3 wo;
                                        pv->primitive->SampleDirection(rng->Next2D(), rng->Next(), pv->type, pv->geom, wi, wo);
                                        return wo;
                                    }
                                }();

                                //#if LM_PT_MANIFOLDNEE_DEBUG_IO
                                //if (numVertices == 3)
                                //{
                                //    LM_LOG_DEBUG("ray");
                                //    DebugIO::Wait();
                                //    std::vector<double> vs;
                                //    const auto p1 = pv->geom.p;
                                //    const auto p2 = pv->geom.p + wo * 0.5_f;
                                //    for (int j = 0; j < 3; j++) { vs.push_back(p1[j]); }
                                //    for (int j = 0; j < 3; j++) { vs.push_back(p2[j]); }
                                //    std::stringstream ss; { cereal::JSONOutputArchive oa(ss); oa(vs); }
                                //    DebugIO::Output("ray", ss.str());
                                //}
                                //#endif
                                
                                // Intersection query
                                Ray ray = { pv->geom.p, wo };
                                Intersection isect;
                                if (!scene->Intersect(ray, isect))
                                {
                                    break;
                                }

                                // Add vertex
                                SubpathSampler::PathVertex v;
                                v.type = isect.primitive->Type() & ~SurfaceInteractionType::Emitter;
                                v.geom = isect.geom;
                                v.primitive = isect.primitive;
                                subpathL.vertices.push_back(v);

                                // Stop if intersected with non specular vertex
                                if ((v.type & SurfaceInteractionType::S) == 0)
                                {
                                    break;
                                }

                                // Terminate path with infinite geometry
                                if (v.geom.infinite)
                                {
                                    break;
                                }
                            }
                        }
                        {
                            // Fails if the last vertex is specular or E or inifinite geometry
                            const auto& v = subpathL.vertices.back();
                            if (v.geom.infinite || (v.type & SurfaceInteractionType::S) > 0 || (v.type & SurfaceInteractionType::E) > 0)
                            {
                                return boost::none;
                            }
                        }
                        return subpathL;
                    }();
                    if (!subpathL)
                    {
                        return;
                    }

                    // --------------------------------------------------------------------------------

                    //#if LM_PT_MANIFOLDNEE_DEBUG_IO
                    //{
                    //    LM_LOG_DEBUG("manifoldnee_seed_path");
                    //    DebugIO::Wait();
                    //    std::vector<double> vs;
                    //    for (const auto& v : subpathL->vertices) for (int i = 0; i < 3; i++) { vs.push_back(v.geom.p[i]); }
                    //    std::stringstream ss; { cereal::JSONOutputArchive oa(ss); oa(vs); }
                    //    DebugIO::Output("manifoldnee_seed_path", ss.str());
                    //}
                    //#endif

                    // --------------------------------------------------------------------------------

                    // Evaluate contribution
                    const auto C = [&]() -> boost::optional<SPD>
                    {
                        if (subpathL->vertices.size() == 1 || subpathL->vertices.size() == 2)
                        {
                            // NEE
                            const auto& vL = subpathL->vertices[0];
                            const auto pdfL = scene->EvaluateEmitterPDF(vL.primitive);
                            const auto pdfPL = vL.primitive->EvaluatePositionGivenPreviousPositionPDF(vL.geom, geom, false);
                            const auto ppL = Math::Normalize(vL.geom.p - geom.p);
                            const auto fsE = primitive->EvaluateDirection(geom, type, wi, ppL, TransportDirection::EL, true);
                            const auto fsL = vL.primitive->EvaluateDirection(vL.geom, SurfaceInteractionType::L, Vec3(), -ppL, TransportDirection::LE, false);
                            const auto G = RenderUtils::GeometryTerm(geom, vL.geom);
                            const auto V = scene->Visible(geom.p, vL.geom.p) ? 1_f : 0_f;
                            const auto LeP = vL.primitive->EvaluatePosition(vL.geom, false);
                            const auto C = throughput * fsE * G * V * fsL * LeP / pdfL / pdfPL;
                            return C;
                        }
                        else
                        {
                            // Manifold NEE
                            const auto connPath = ManifoldUtils::WalkManifold(scene, *subpathL, geom.p);
                            if (!connPath) { return boost::none; }
                            const auto connPathInv = ManifoldUtils::WalkManifold(scene, *connPath, subpathL->vertices.back().geom.p);
                            if (!connPathInv) { return boost::none; }

                            // Contribution
                            const auto& vL  = connPath->vertices[0];
                            const auto pdfL = scene->EvaluateEmitterPDF(vL.primitive);
                            const auto pdfPL = vL.primitive->EvaluatePositionGivenPreviousPositionPDF(vL.geom, geom, false);
                            const auto LeP = vL.primitive->EvaluatePosition(vL.geom, false);
                            const auto fsE = primitive->EvaluateDirection(geom, type, wi, Math::Normalize(connPath->vertices[connPath->vertices.size()-2].geom.p - geom.p), TransportDirection::EL, true);
                            const auto fsL = vL.primitive->EvaluateDirection(vL.geom, SurfaceInteractionType::L, Vec3(), Math::Normalize(connPath->vertices[1].geom.p - vL.geom.p), TransportDirection::LE, false);
                            const auto fsS = [&]() -> SPD
                            {
                                SPD prodFs(1_f);
                                const int n = (int)(connPath->vertices.size());
                                for (int i = 1; i < n - 1; i++)
                                {
                                    const auto& vi  = connPath->vertices[i];
                                    const auto& vip = connPath->vertices[i - 1];
                                    const auto& vin = connPath->vertices[i + 1];
                                    assert(vi.type == SurfaceInteractionType::S);
                                    const auto wi = Math::Normalize(vip.geom.p - vi.geom.p);
                                    const auto wo = Math::Normalize(vin.geom.p - vi.geom.p);
                                    const auto fs = vi.primitive->EvaluateDirection(vi.geom, vi.type, wi, wo, TransportDirection::LE, false);
                                    prodFs *= fs;
                                }
                                return prodFs;
                            }();
                            const auto multiG = [&]() -> Float
                            {
                                const auto det = ManifoldUtils::ComputeConstraintJacobianDeterminant(*connPath);
                                const auto G = RenderUtils::GeometryTerm(connPath->vertices[0].geom, connPath->vertices[1].geom);
                                return det * G;
                            }();
                            const auto C = throughput * fsE * multiG * fsS * fsL * LeP / pdfL / pdfPL;
                            return C;
                        }
                        LM_UNREACHABLE();
                        return Vec3();
                    }();
                    if (!C || C->Black())
                    {
                        return;
                    }

                    // --------------------------------------------------------------------------------

                    // Record to film
                    auto rp = rasterPos;
                    if (type == SurfaceInteractionType::E) { primitive->sensor->RasterPosition(Math::Normalize(subpathL->vertices[0].geom.p - geom.p), geom, rp); }
                    film->Splat(rp, *C);
                }();
                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Sample next direction
                Vec3 wo;
                if (type == SurfaceInteractionType::E)
                {
                    wo = initWo;
                }
                else
                {
                    primitive->SampleDirection(rng->Next2D(), rng->Next(), type, geom, wi, wo);
                }
                const auto pdfD = primitive->EvaluateDirectionPDF(geom, type, wi, wo, false);
                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Calculate pixel index for initial vertex
                if (type == SurfaceInteractionType::E)
                {
                    if (!primitive->sensor->RasterPosition(wo, geom, rasterPos))
                    {
                        break;
                    }
                }
                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Evaluate direction
                const auto fs = primitive->EvaluateDirection(geom, type, wi, wo, TransportDirection::EL, false);
                if (fs.Black())
                {
                    break;
                }
                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Update throughput
                assert(pdfD > 0_f);
                throughput *= fs / pdfD;
                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Intersection
                Ray ray = { geom.p, wo };
                Intersection isect;
                if (!scene->Intersect(ray, isect))
                {
                    break;
                }
                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Handle hit with light source for LS*D paths
                if ((isect.primitive->Type() & SurfaceInteractionType::L) > 0 && lastNonSIndex == 0)
                {
                    const auto C =
                        throughput
                        * isect.primitive->EvaluateDirection(isect.geom, SurfaceInteractionType::L, Vec3(), -ray.d, TransportDirection::EL, false)
                        * isect.primitive->EvaluatePosition(isect.geom, false);
                    film->Splat(rasterPos, C);
                }
                if ((isect.primitive->Type() & SurfaceInteractionType::S) == 0)
                {
                    lastNonSIndex = numVertices;
                }
                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Path termination
                if (isect.geom.infinite)
                {
                    break;
                }
                Float rrProb = 0.5_f;
                if (rng->Next() > rrProb)
                {
                    break;
                }
                else
                {
                    throughput /= rrProb;
                }
                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Update information
                geom = isect.geom;
                primitive = isect.primitive;
                type = isect.primitive->Type() & ~SurfaceInteractionType::Emitter;
                wi = -ray.d;
                numVertices++;
                #pragma endregion
            }
        });

        // --------------------------------------------------------------------------------

        #pragma region Save image
        {
            LM_LOG_INFO("Saving image");
            LM_LOG_INDENTER();
            film_->Save(outputPath);
        }
        #pragma endregion

        // --------------------------------------------------------------------------------

        #if LM_PT_MANIFOLDNEE_DEBUG_IO
        DebugIO::Stop();
        #endif
    };

private:

    int maxNumVertices_;
    Scheduler::UniquePtr sched_;

};

LM_COMPONENT_REGISTER_IMPL(Renderer_PT_ManifoldNEE, "renderer::pt_manifoldnee");

LM_NAMESPACE_END
