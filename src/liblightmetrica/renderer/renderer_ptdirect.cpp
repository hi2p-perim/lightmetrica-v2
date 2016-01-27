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
#include <lightmetrica/surfacegeometry.h>
#include <lightmetrica/primitive.h>
#include <lightmetrica/scheduler.h>
#include <lightmetrica/renderutils.h>

LM_NAMESPACE_BEGIN

class Renderer_PTDirect final : public Renderer
{
public:

    LM_IMPL_CLASS(Renderer_PTDirect, Renderer);


public:

    Renderer_PTDirect()
        : sched_(ComponentFactory::Create<Scheduler>())
    {}

public:

    LM_IMPL_F(Initialize) = [this](const PropertyNode* prop) -> bool
    {
        sched_->Load(prop);
        maxNumVertices_ = prop->Child("max_num_vertices")->As<int>();
        return true;
    };

    LM_IMPL_F(Render) = [this](const Scene* scene, Film* film) -> void
    {
        Random initRng;
        #if LM_DEBUG_MODE
        initRng.SetSeed(1008556906);
        #else
        initRng.SetSeed(static_cast<unsigned int>(std::time(nullptr)));
        #endif

        sched_->Process(scene, film, &initRng, [this](const Scene* scene, Film* film, Random* rng)
        {
            #pragma region Sample a sensor

            const auto* E = scene->SampleEmitter(SurfaceInteraction::E, rng->Next());
            const Float pdfE = scene->EvaluateEmitterPDF(SurfaceInteraction::E);
            assert(pdfE > 0);

            #pragma endregion

            // --------------------------------------------------------------------------------

            #pragma region Sample a position on the sensor

            SurfaceGeometry geomE;
            E->SamplePosition(rng->Next2D(), geomE);
            const Float pdfPE = E->EvaluatePositionPDF(geomE, false);

            #pragma endregion

            // --------------------------------------------------------------------------------

            #pragma region Temporary variables

            auto throughput = E->EvaluatePosition(geomE, false) / pdfPE / pdfE;
            //SPD throughput(1_f);
            const auto* primitive = E;
            int type = SurfaceInteraction::E;
            auto geom = geomE;
            Vec3 wi;
            Vec2 rasterPos;
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

                {
                    #pragma region Sample a light

                    const auto* L = scene->SampleEmitter(SurfaceInteraction::L, rng->Next());
                    const Float pdfL = scene->EvaluateEmitterPDF(SurfaceInteraction::L);
                    assert(pdfL > 0);

                    #pragma endregion

                    // --------------------------------------------------------------------------------

                    #pragma region Sample a position on the light

                    SurfaceGeometry geomL;
                    L->SamplePosition(rng->Next2D(), geomL);
                    const Float pdfPL = L->EvaluatePositionPDF(geomL, false);
                    assert(pdfPL > 0);

                    #pragma endregion

                    // --------------------------------------------------------------------------------

                    #pragma region Evaluate contribution

                    const auto ppL = Math::Normalize(geomL.p - geom.p);
                    const auto fsE = primitive->EvaluateDirection(geom, type, wi, ppL, TransportDirection::EL, true);
                    const auto fsL = L->EvaluateDirection(geomL, SurfaceInteraction::L, Vec3(), -ppL, TransportDirection::LE, true);
                    const auto G = RenderUtils::GeometryTerm(geom, geomL);
                    const auto V = scene->Visible(geom.p, geomL.p) ? 1_f : 0_f;
                    const auto LeP = L->EvaluatePosition(geomL, false);
                    const auto C = throughput * fsE * G * V * fsL * LeP / pdfL / pdfPL;

                    #pragma endregion

                    // --------------------------------------------------------------------------------

                    #pragma region Record to film

                    if (!C.Black())
                    {
                        // Recompute pixel index if necessary
                        auto rp = rasterPos;
                        if (type == SurfaceInteraction::E)
                        {
                            primitive->emitter->RasterPosition(ppL, geom, rp);
                        }

                        // Accumulate to film
                        film->Splat(rp, C);
                        //film->Splat(rasterPos, SPD(1_f));
                    }

                    #pragma endregion
                }

                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Sample next direction

                Vec3 wo;
                primitive->SampleDirection(rng->Next2D(), rng->Next(), type, geom, wi, wo);
                const Float pdfD = primitive->EvaluateDirectionPDF(geom, type, wi, wo, false);

                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Calculate pixel index for initial vertex

                if (type == SurfaceInteraction::E)
                {
                    if (!primitive->emitter->RasterPosition(wo, geom, rasterPos))
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

                assert(pdfD > 0);
                throughput *= fs / pdfD;

                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Intersection

                // Setup next ray
                Ray ray = { geom.p, wo };

                // Intersection query
                Intersection isect;
                if (!scene->Intersect(ray, isect))
                {
                    break;
                }

                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Path termination

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
                type = isect.primitive->Type() & ~SurfaceInteraction::Emitter;
                wi = -ray.d;
                numVertices++;

                #pragma endregion
            }
        });
    };

private:

    int maxNumVertices_;
    Scheduler::UniquePtr sched_;

};

LM_COMPONENT_REGISTER_IMPL(Renderer_PTDirect, "renderer::ptdirect");

LM_NAMESPACE_END
