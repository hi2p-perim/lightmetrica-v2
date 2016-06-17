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

#pragma once

#include <lightmetrica/lightmetrica.h>
#include <lightmetrica/detail/parallel.h>

LM_NAMESPACE_BEGIN

struct PathVertex
{
    int type;
    SurfaceGeometry geom;
    const Primitive* primitive = nullptr;
};

struct Path
{
    std::vector<PathVertex> vertices;

    auto EvaluateF(int s) const -> SPD
    {
        const int n = (int)(vertices.size());
        const int t = n - s;
        assert(n >= 2);

        // --------------------------------------------------------------------------------

        SPD fL;
        if (s == 0) { fL = SPD(1_f); }
        else
        {
            {
                const auto* vL = &vertices[0];
                fL = vL->primitive->emitter->EvaluatePosition(vL->geom, false);
            }
            for (int i = 0; i < s - 1; i++)
            {
                const auto* v = &vertices[i];
                const auto* vPrev = i >= 1 ? &vertices[i - 1] : nullptr;
                const auto* vNext = &vertices[i + 1];
                const auto wi = vPrev ? Math::Normalize(vPrev->geom.p - v->geom.p) : Vec3();
                const auto wo = Math::Normalize(vNext->geom.p - v->geom.p);
                fL *= v->primitive->surface->EvaluateDirection(v->geom, v->type, wi, wo, TransportDirection::LE, false);
                fL *= RenderUtils::GeometryTerm(v->geom, vNext->geom);
            }
        }
        if (fL.Black()) { return SPD(); }

        // --------------------------------------------------------------------------------

        SPD fE;
        if (t == 0) { fE = SPD(1_f); }
        else
        {
            {
                const auto* vE = &vertices[n - 1];
                fE = vE->primitive->emitter->EvaluatePosition(vE->geom, false);
            }
            for (int i = n - 1; i > s; i--)
            {
                const auto* v = &vertices[i];
                const auto* vPrev = &vertices[i - 1];
                const auto* vNext = i < n - 1 ? &vertices[i + 1] : nullptr;
                const auto wi = vNext ? Math::Normalize(vNext->geom.p - v->geom.p) : Vec3();
                const auto wo = Math::Normalize(vPrev->geom.p - v->geom.p);
                fE *= v->primitive->surface->EvaluateDirection(v->geom, v->type, wi, wo, TransportDirection::EL, false);
                fE *= RenderUtils::GeometryTerm(v->geom, vPrev->geom);
            }
        }
        if (fE.Black()) { return SPD(); }

        // --------------------------------------------------------------------------------

        SPD cst;
        if (s == 0 && t > 0)
        {
            const auto& v = vertices[0];
            const auto& vNext = vertices[1];
            cst = v.primitive->emitter->EvaluatePosition(v.geom, true) * v.primitive->emitter->EvaluateDirection(v.geom, v.type, Vec3(), Math::Normalize(vNext.geom.p - v.geom.p), TransportDirection::EL, false);
        }
        else if (s > 0 && t == 0)
        {
            const auto& v = vertices[n - 1];
            const auto& vPrev = vertices[n - 2];
            cst = v.primitive->emitter->EvaluatePosition(v.geom, true) * v.primitive->emitter->EvaluateDirection(v.geom, v.type, Vec3(), Math::Normalize(vPrev.geom.p - v.geom.p), TransportDirection::LE, false);
        }
        else if (s > 0 && t > 0)
        {
            const auto* vL = &vertices[s - 1];
            const auto* vE = &vertices[s];
            const auto* vLPrev = s - 2 >= 0 ? &vertices[s - 2] : nullptr;
            const auto* vENext = s + 1 < n ? &vertices[s + 1] : nullptr;
            const auto fsL = vL->primitive->surface->EvaluateDirection(vL->geom, vL->type, vLPrev ? Math::Normalize(vLPrev->geom.p - vL->geom.p) : Vec3(), Math::Normalize(vE->geom.p - vL->geom.p), TransportDirection::LE, true);
            const auto fsE = vE->primitive->surface->EvaluateDirection(vE->geom, vE->type, vENext ? Math::Normalize(vENext->geom.p - vE->geom.p) : Vec3(), Math::Normalize(vL->geom.p - vE->geom.p), TransportDirection::EL, true);
            const Float G = RenderUtils::GeometryTerm(vL->geom, vE->geom);
            cst = fsL * G * fsE;
        }

        // --------------------------------------------------------------------------------

        return fL * cst * fE;
    }

    auto EvaluatePathPDF(const Scene* scene, int s) const -> PDFVal
    {
        const int n = (int)(vertices.size());
        const int t = n - s;
        assert(n >= 2);

        // Check if the path is samplable by vertex connection
        if (s == 0 && t > 0)
        {
            const auto& v = vertices[0];
            if (v.primitive->emitter->IsDeltaPosition(v.type)) { return PDFVal(PDFMeasure::ProdArea, 0_f); }
        }
        else if (s > 0 && t == 0)
        {
            const auto& v = vertices[n - 1];
            if (v.primitive->emitter->IsDeltaPosition(v.type)) { return PDFVal(PDFMeasure::ProdArea, 0_f); }
        }
        else if (s > 0 && t > 0)
        {
            const auto& vL = vertices[s - 1];
            const auto& vE = vertices[s];
            if (vL.primitive->surface->IsDeltaDirection(vL.type) || vE.primitive->surface->IsDeltaDirection(vE.type)) { return PDFVal(PDFMeasure::ProdArea, 0_f); }
        }

        // Otherwise the path can be generated with the given strategy (s,t,merge) so p_{s,t,merge} can be safely evaluated.
        PDFVal pdf(PDFMeasure::ProdArea, 1_f);
        if (s > 0)
        {
            pdf *= vertices[0].primitive->emitter->EvaluatePositionGivenDirectionPDF(vertices[0].geom, Math::Normalize(vertices[1].geom.p - vertices[0].geom.p), false) * scene->EvaluateEmitterPDF(vertices[0].primitive).v;
            for (int i = 0; i < s - 1; i++)
            {
                const auto* vi = &vertices[i];
                const auto* vip = i - 1 >= 0 ? &vertices[i - 1] : nullptr;
                const auto* vin = &vertices[i + 1];
                pdf *= vi->primitive->surface->EvaluateDirectionPDF(vi->geom, vi->type, vip ? Math::Normalize(vip->geom.p - vi->geom.p) : Vec3(), Math::Normalize(vin->geom.p - vi->geom.p), false).ConvertToArea(vi->geom, vin->geom);
            }
        }
        if (t > 0)
        {
            pdf *= vertices[n - 1].primitive->emitter->EvaluatePositionGivenDirectionPDF(vertices[n - 1].geom, Math::Normalize(vertices[n - 2].geom.p - vertices[n - 1].geom.p), false) * scene->EvaluateEmitterPDF(vertices[n - 1].primitive).v;
            for (int i = n - 1; i >= s + 1; i--)
            {
                const auto* vi = &vertices[i];
                const auto* vip = &vertices[i - 1];
                const auto* vin = i + 1 < n ? &vertices[i + 1] : nullptr;
                pdf *= vi->primitive->surface->EvaluateDirectionPDF(vi->geom, vi->type, vin ? Math::Normalize(vin->geom.p - vi->geom.p) : Vec3(), Math::Normalize(vip->geom.p - vi->geom.p), false).ConvertToArea(vi->geom, vip->geom);
            }
        }

        return pdf;
    }

    auto RasterPosition() const -> Vec2
    {
        const auto& v = vertices[vertices.size() - 1];
        const auto& vPrev = vertices[vertices.size() - 2];
        Vec2 rasterPos;
        v.primitive->sensor->RasterPosition(Math::Normalize(vPrev.geom.p - v.geom.p), v.geom, rasterPos);
        return rasterPos;
    }

};

// --------------------------------------------------------------------------------

///! Utility class for inversemap project
class InversemapUtils
{
public:

    LM_DISABLE_CONSTRUCT(InversemapUtils);

public:

    static auto MapPS2Path(const Scene& scene, const std::vector<Float>& primarySample) const -> Path
    {
        Vec3 initWo;
        PathVertex pv, ppv;
        Path path;
        int samplerIndex = 0;
        const int maxNumVertices = (int)(primarySample.size()) / 2;
        for (int step = 0; step < maxNumVertices; step++)
        {
            if (step == 0)
            {
                //region Sample initial vertex

                PathVertex v;

                // Emitter is fixed (initial one is used)
                v.type = transDir == TransportDirection::LE ? SurfaceInteractionType::L : SurfaceInteractionType::E;
                v.primitive = scene.GetSensor();

                // Assume the sensor is pinhole camera
                assert(std::strcmp(v.primitive->emitter->implName, "Sensor_Pinhole") == 0);

                // Sample a position on the emitter and initial ray direction
                v.primitive->emitter->SamplePositionAndDirection(rng->Next2D(), Vec2(), v.geom, initWo);

                // Create a vertex
                path.vertices.push_back(v);

                // Update information
                pv = v;

                //endregion
            }
            else
            {
                //region Sample intermediate vertex

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
                    pv.primitive->surface->SampleDirection(rng->Next2D(), rng->Next(), pv.type, pv.geom, wi, wo);
                }

                // Intersection query
                Ray ray = { pv.geom.p, wo };
                Intersection isect;
                if (!scene->Intersect(ray, isect))
                {
                    break;
                }

                // Assume all surface is diffuse
                assert(std::strcmp(isect.primitive->bsdf->implName, "BSDF_Diffuse") == 0);

                // Add a vertex
                PathVertex v;
                v.geom = isect.geom;
                v.primitive = isect.primitive;
                v.type = isect.primitive->surface->Type() & ~SurfaceInteractionType::Emitter;
                path.vertices.push_back(v);

                // Path termination
                if (isect.geom.infinite)
                {
                    break;
                }

                // Update information
                ppv = pv;
                pv = v;

                //endregion
            }
        }

        if ((path.vertices.back().primitive->type & PrimitiveType::L) > 0)
        {
            path.vertices.back().type = PrimitiveType::L;
        }

        std::reverse(path.vertices.begin(), path.vertices.end());

        assert(path.vertices.size() == primarySample.size());

        return path;
    }

    static auto MapPath2PS(const Path& inputPath) const -> std::vector<Float>
    {
        //region Helper function
        const auto UniformConcentricDiskSample_Inverse = [](const Vec2& s) -> Vec2
        {
            const auto r = std::sqrt(s.x*s.x + s.y*s.y);
            auto theta = std::atan2(s.y, s.x);
            Vec2 u;
            if (s.x > -s.y)
            {
                if (s.x > s.y)
                {
                    u.x = r;
                    u.y = 4_f * theta * r * Math::InvPi();
                }
                else
                {
                    u.y = r;
                    u.x = (2_f - 4_f * theta * Math::InvPi()) * r;
                }
            }
            else
            {
                theta = theta < 0_f ? theta + 2_f * Math::Pi() : theta;
                if (s.x < s.y)
                {
                    u.x = -r;
                    u.y = (4_f - 4_f * theta * Math::InvPi()) * r;
                }
                else
                {
                    u.y = -r;
                    u.x = (-6_f + 4_f * theta * Math::InvPi()) * r;
                }
            }
            return (u + Vec2(1_f)) * 0.5_f;
        };
        //endregion

        // --------------------------------------------------------------------------------

        std::vector<Float> ps;
        auto path = inputPath;
        std::reverse(path.vertices.begin(), path.vertices.end());

        for (size_t i = 0; i < path.vertices.size(); i++)
        {
            const auto* v = &path.vertices[i];
            const auto* vn = i + 1 < path.vertices.size() ? &path.vertices[i + 1] : nullptr;

            if (i == 0)
            {
                // No sample is needed for the pinhole camera
                assert(std::strcmp(v->primitive->emitter->implName, "Sensor_Pinhole") == 0);
            }

            if (vn)
            {
                assert(v->type == SurfaceInteractionType::E || v->type == SurfaceInteractionType::D);
                if (v->type == SurfaceInteractionType::E)
                {
                    //
                    
                }
                else if (v->type == SurfaceInteractionType::D)
                {
                    const auto wo = Math::Normalize(vn->geom.p - v->geom.p);
                    const auto localWo = v->geom.ToLocal * wo;
                    const auto inv = UniformConcentricDiskSample_Inverse(Vec2(localWo.x, localWo.y));
                    ps.push_back(inv.x);
                    ps.push_back(inv.y);
                }
            }
        }

        return ps;
    }

};

LM_NAMESPACE_END