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

#include <pch.h>
#include <lightmetrica/renderer.h>
#include <lightmetrica/property.h>
#include <lightmetrica/random.h>
#include <lightmetrica/scene.h>
#include <lightmetrica/film.h>
#include <lightmetrica/bsdf.h>
#include <lightmetrica/ray.h>
#include <lightmetrica/intersection.h>
#include <lightmetrica/emitter.h>
#include <lightmetrica/sensor.h>
#include <lightmetrica/surfacegeometry.h>
#include <lightmetrica/primitive.h>
#include <lightmetrica/scheduler.h>
#include <lightmetrica/detail/parallel.h>

#define LM_PTMIS_DEBUG_WEIGHT_IMAGE 0
#define LM_PTMIS_DEBUG_SIMPLIFY_PT_ONLY 0
#define LM_PTMIS_DEBUG_SIMPLIFY_DIRECT_ONLY 0

LM_NAMESPACE_BEGIN

class Renderer_PTMIS final : public Renderer
{
public:

    LM_IMPL_CLASS(Renderer_PTMIS, Renderer);

private:

    int maxNumVertices_;
    int minNumVertices_;
    Scheduler::UniquePtr sched_ = ComponentFactory::Create<Scheduler>();

public:

    LM_IMPL_F(Initialize) = [this](const PropertyNode* prop) -> bool
    {
        sched_->Load(prop);
        maxNumVertices_ = prop->ChildAs("max_num_vertices", -1);
        minNumVertices_ = prop->ChildAs("min_num_vertices", 0);
        return true;
    };

    LM_IMPL_F(Render) = [this](const Scene* scene_, Random* initRng, const std::string& outputPath) -> void
    {
        #if LM_PTMIS_DEBUG_WEIGHT_IMAGE
        assert(Parallel::GetNumThreads() == 1);
        auto filmW1 = ComponentFactory::Clone<Film>(film_);
        auto filmW2 = ComponentFactory::Clone<Film>(film_);
        #endif

        const auto* scene = static_cast<const Scene3*>(scene_);
        auto* film_ = static_cast<const Sensor*>(scene->GetSensor()->emitter)->GetFilm();
        const long long processed = sched_->Process(scene, film_, initRng, [&](Film* film, Random* rng)
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

            #pragma region Calculate raster position for initial vertex
            Vec2 rasterPos;
            if (!E->RasterPosition(initWo, geomE, rasterPos))
            {
                // This can happen due to numerical errors
                return;
            }
            #pragma endregion

            // --------------------------------------------------------------------------------

            #pragma region Temporary variables
            auto throughput = E->sensor->EvaluatePosition(geomE, false) / pdfPE / pdfE;
            const auto* primitive = E;
            int type = SurfaceInteractionType::E;
            auto geom = geomE;
            Vec3 wi;
            int numVertices = 1;
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
                #if !LM_PTMIS_DEBUG_SIMPLIFY_PT_ONLY
                if (numVertices + 1 >= minNumVertices_)
                {
                    #pragma region Sample a light
                    const auto* L = scene->SampleEmitter(SurfaceInteractionType::L, rng->Next());
                    const auto pdfL = scene->EvaluateEmitterPDF(L);
                    assert(pdfL > 0_f);
                    #pragma endregion

                    // --------------------------------------------------------------------------------

                    #pragma region Sample a position on the light
                    SurfaceGeometry geomL;
                    L->SamplePositionGivenPreviousPosition(rng->Next2D(), geom, geomL);
                    const auto pdfPL = L->EvaluatePositionGivenPreviousPositionPDF(geomL, geom, false);
                    assert(pdfPL > 0_f);
                    #pragma endregion

                    // --------------------------------------------------------------------------------

                    #pragma region Evaluate contribution
                    const auto ppL = Math::Normalize(geomL.p - geom.p);
                    const auto fsE = primitive->EvaluateDirection(geom, type, wi, ppL, TransportDirection::EL, true);
                    const auto fsL = L->EvaluateDirection(geomL, SurfaceInteractionType::L, Vec3(), -ppL, TransportDirection::LE, false);
                    const auto G = RenderUtils::GeometryTerm(geom, geomL);
                    const auto V = scene->Visible(geom.p, geomL.p) ? 1_f : 0_f;
                    const auto LeP = L->EvaluatePosition(geomL, false);
                    const auto C = throughput * fsE * G * V * fsL * LeP / pdfL / pdfPL;
                    #pragma endregion

                    // --------------------------------------------------------------------------------

                    #pragma region MIS
                    #if LM_PTMIS_DEBUG_SIMPLIFY_DIRECT_ONLY
                    const auto w = 1_f;
                    #else
                    const auto pdfD_DirectLight = pdfPL.ConvertToProjSA(geom, geomL).v * pdfL.v;
                    const auto pdfD_BSDF = primitive->EvaluateDirectionPDF(geom, type, wi, ppL, true).v;
                    const auto w = pdfD_DirectLight / (pdfD_DirectLight + pdfD_BSDF);
                    #endif
                    #pragma endregion

                    // --------------------------------------------------------------------------------

                    #pragma region Record to film
                    if (!C.Black())
                    {
                        // Recompute pixel index if necessary
                        auto rp = rasterPos;
                        if (type == SurfaceInteractionType::E)
                        {
                            primitive->sensor->RasterPosition(ppL, geom, rp);
                        }

                        // Accumulate to film
                        film->Splat(rp, w * C);

                        #if LM_PTMIS_DEBUG_WEIGHT_IMAGE
                        filmW1->Splat(rp, SPD(w));
                        #endif
                    }
                    #pragma endregion
                }
                #endif
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

                #pragma region Handle hit with light source
                #if !LM_PTMIS_DEBUG_SIMPLIFY_DIRECT_ONLY
                if ((isect.primitive->Type() & SurfaceInteractionType::L) > 0)
                {
                    // Accumulate to film
                    if (numVertices + 1 >= minNumVertices_)
                    {
                        // MIS weight
                        #if LM_PTMIS_DEBUG_SIMPLIFY_PT_ONLY
                        const auto w = 1_f;
                        #else
                        const auto pdfD_BSDF = pdfD.v;
                        const auto pdfD_DirectLight =
                            (type & SurfaceInteractionType::S) > 0
                                ? 0_f
                                : isect.primitive->EvaluatePositionGivenPreviousPositionPDF(isect.geom, geom, true).ConvertToProjSA(isect.geom, geom).v *
                                  scene->EvaluateEmitterPDF(isect.primitive).v;
                        const auto w = pdfD_BSDF / (pdfD_BSDF + pdfD_DirectLight);
                        #endif

                        // Contribution
                        const auto C =
                            throughput
                            * isect.primitive->EvaluateDirection(isect.geom, SurfaceInteractionType::L, Vec3(), -ray.d, TransportDirection::EL, false)
                            * isect.primitive->EvaluatePosition(isect.geom, false);
                        film->Splat(rasterPos, w * C);

                        #if LM_PTMIS_DEBUG_WEIGHT_IMAGE
                        filmW2->Splat(rasterPos, SPD(w));
                        #endif
                    }
                }
                #endif
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

        #if LM_PTMIS_DEBUG_WEIGHT_IMAGE
        filmW1->Rescale(1_f / processed);
        filmW2->Rescale(1_f / processed);
        filmW1->Save("ptmis_w1");
        filmW2->Save("ptmis_w2");
        #else
        LM_UNUSED(processed);
        #endif

        // --------------------------------------------------------------------------------

        #pragma region Save image
        {
            LM_LOG_INFO("Saving image");
            LM_LOG_INDENTER();
            film_->Save(outputPath);
        }
        #pragma endregion
    };

};

LM_COMPONENT_REGISTER_IMPL(Renderer_PTMIS, "renderer::ptmis");

LM_NAMESPACE_END


