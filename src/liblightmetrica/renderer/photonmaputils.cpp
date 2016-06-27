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
#include <lightmetrica/detail/photonmaputils.h>
#include <lightmetrica/detail/photonmap.h>
#include <lightmetrica/logger.h>
#include <lightmetrica/random.h>
#include <lightmetrica/light.h>
#include <lightmetrica/sensor.h>
#include <lightmetrica/ray.h>
#include <lightmetrica/intersection.h>
#include <lightmetrica/scene.h>
#include <lightmetrica/primitive.h>

LM_NAMESPACE_BEGIN

namespace
{
    auto TraceSubpath_(
        const Scene* scene,
        Random* rng,
        int maxNumVertices,
        TransportDirection transDir,
        boost::optional<Vec2> initRasterPos,
        const std::function<bool(int step, const Vec2&, const PhotonMapUtils::PathVertex&, const PhotonMapUtils::PathVertex&, SPD&)>& processPathVertexFunc) -> void
    {
        Vec3 initWo;
        PhotonMapUtils::PathVertex pv, ppv;
        SPD throughput;
        Vec2 rasterPos;
        for (int step = 0; maxNumVertices == -1 || step < maxNumVertices; step++)
        {
            if (step == 0)
            {
                #pragma region Sample initial vertex

                PhotonMapUtils::PathVertex v;

                // Sample an emitter
                v.type = transDir == TransportDirection::LE ? SurfaceInteractionType::L : SurfaceInteractionType::E;
                v.primitive = scene->SampleEmitter(v.type, rng->Next());

                // Sample a position on the emitter and initial ray direction
                v.primitive->SamplePositionAndDirection(initRasterPos ? *initRasterPos : rng->Next2D(), rng->Next2D(), v.geom, initWo);

                // Initial throughput
                throughput =
                    v.primitive->EvaluatePosition(v.geom, false) /
                    v.primitive->EvaluatePositionGivenDirectionPDF(v.geom, initWo, false) /
                    scene->EvaluateEmitterPDF(v.primitive);

                // Raster position
                if (transDir == TransportDirection::EL)
                {
                    v.primitive->sensor->RasterPosition(initWo, v.geom, rasterPos);
                }

                // Process vertex
                if (!processPathVertexFunc(1, rasterPos, PhotonMapUtils::PathVertex(), v, throughput))
                {
                    break;
                }

                // Update information
                pv = v;

                #pragma endregion
            }
            else
            {
                #pragma region Sample a vertex with PDF with BSDF

                // Sample a next direction
                Vec3 wi;
                Vec3 wo;
                if (step == 1)
                {
                    wi = Vec3();
                    wo = initWo;
                }
                else
                {
                    wi = Math::Normalize(ppv.geom.p - pv.geom.p);
                    pv.primitive->SampleDirection(rng->Next2D(), rng->Next(), pv.type, pv.geom, wi, wo);
                }

                // Evaluate direction
                const auto fs = pv.primitive->EvaluateDirection(pv.geom, pv.type, wi, wo, transDir, false);
                if (fs.Black())
                {
                    break;
                }
                const auto pdfD = pv.primitive->EvaluateDirectionPDF(pv.geom, pv.type, wi, wo, false);
                assert(pdfD > 0_f);

                // Update throughput
                throughput *= fs / pdfD;

                // Intersection query
                Ray ray = { pv.geom.p, wo };
                Intersection isect;
                if (!scene->Intersect(ray, isect))
                {
                    break;
                }

                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Process path vertex

                PhotonMapUtils::PathVertex v;
                v.geom = isect.geom;
                v.primitive = isect.primitive;
                v.type = isect.primitive->Type() & ~SurfaceInteractionType::Emitter;
                if (!processPathVertexFunc(step + 1, rasterPos, pv, v, throughput))
                {
                    break;
                }

                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Path termiantion
                
                if (isect.geom.infinite)
                {
                    break;
                }

                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Update information

                ppv = pv;
                pv = v;

                #pragma endregion
            }
        }
    }
}

auto PhotonMapUtils::TraceSubpath(const Scene* scene, Random* rng, int maxNumVertices, TransportDirection transDir, const std::function<bool(int step, const Vec2&, const PhotonMapUtils::PathVertex&, const PhotonMapUtils::PathVertex&, SPD&)>& processPathVertexFunc) -> void
{
    TraceSubpath_(scene, rng, maxNumVertices, transDir, boost::none, processPathVertexFunc);
}

auto PhotonMapUtils::TraceEyeSubpathFixedRasterPos(const Scene* scene, Random* rng, int maxNumVertices, TransportDirection transDir, const Vec2& rasterPos, const std::function<bool(int step, const Vec2&, const PhotonMapUtils::PathVertex&, const PhotonMapUtils::PathVertex&, SPD&)>& processPathVertexFunc) -> void
{
    assert(transDir == TransportDirection::EL);
    TraceSubpath_(scene, rng, maxNumVertices, transDir, rasterPos, processPathVertexFunc);
}

LM_NAMESPACE_END