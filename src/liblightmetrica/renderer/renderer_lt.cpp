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
#include <lightmetrica/light.h>
#include <lightmetrica/surfacegeometry.h>
#include <lightmetrica/primitive.h>
#include <lightmetrica/scheduler.h>

LM_NAMESPACE_BEGIN

class Renderer_LT final : public Renderer
{
public:

    LM_IMPL_CLASS(Renderer_LT, Renderer);

private:

    int maxNumVertices_;
    Scheduler::UniquePtr sched_;

public:

    Renderer_LT()
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
        auto* film_ = static_cast<const Sensor*>(scene->GetSensor()->emitter)->GetFilm();
        sched_->Process(scene, film_, initRng, [&](Film* film, Random* rng)
        {
            #pragma region Sample a light

            const auto* L = scene->SampleEmitter(SurfaceInteractionType::L, rng->Next());
            const auto pdfL = scene->EvaluateEmitterPDF(L);
            assert(pdfL.v > 0);

            #pragma endregion

            // --------------------------------------------------------------------------------

            #pragma region Sample a position on the light

            SurfaceGeometry geomL;
            Vec3 initWo;
            L->SamplePositionAndDirection(rng->Next2D(), rng->Next2D(), geomL, initWo);
            const auto pdfPL = L->EvaluatePositionGivenDirectionPDF(geomL, initWo, false);
            assert(pdfPL.v > 0);

            #pragma endregion

            // --------------------------------------------------------------------------------

            #pragma region Temporary variables

            auto throughput = L->EvaluatePosition(geomL, false) / pdfPL / pdfL;
            const auto* primitive = L;
            int type = SurfaceInteractionType::L;
            auto geom = geomL;
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

                #pragma region Sample direction

                Vec3 wo;
                if (type == SurfaceInteractionType::L)
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

                const auto fs = primitive->EvaluateDirection(geom, type, wi, wo, TransportDirection::LE, false);
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

                #pragma region Handle hit with sensor

                if ((isect.primitive->Type() & SurfaceInteractionType::E) > 0)
                {
                    #pragma region Calculate raster position

                    Vec2 rasterPos;
                    if (!isect.primitive->sensor->RasterPosition(-wo, isect.geom, rasterPos))
                    {
                        break;
                    }

                    #pragma endregion

                    // --------------------------------------------------------------------------------

                    #pragma region Accumulate to film

                    const auto C =
                        throughput
                        * isect.primitive->EvaluateDirection(isect.geom, SurfaceInteractionType::E, Vec3(), -ray.d, TransportDirection::LE, true)
                        * isect.primitive->EvaluatePosition(isect.geom, true);
                    film->Splat(rasterPos, C);

                    #pragma endregion
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
    };

};

LM_COMPONENT_REGISTER_IMPL(Renderer_LT, "renderer::lt");

LM_NAMESPACE_END
