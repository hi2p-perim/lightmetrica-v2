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
#include <lightmetrica/light.h>
#include <lightmetrica/property.h>
#include <lightmetrica/trianglemesh.h>
#include <lightmetrica/dist.h>
#include <lightmetrica/primitive.h>
#include <lightmetrica/sampler.h>
#include <lightmetrica/surfacegeometry.h>
#include <lightmetrica/triangleutils.h>
#include <lightmetrica/scene3.h>

LM_NAMESPACE_BEGIN

class EmitterShape_DirectionalLight final : public EmitterShape
{
public:

    LM_IMPL_CLASS(EmitterShape_DirectionalLight, EmitterShape);

public:

    EmitterShape_DirectionalLight(const SphereBound& bound)
        : bound_(bound)
    {}

public:

    LM_IMPL_F(Intersect) = [this](const Ray& ray, Float minT, Float maxT, Intersection& isect) -> bool
    {
        // Intersection with bounding sphere
        Float t;
        if (!bound_.Intersect(ray, minT, maxT, t))
        {
            return false;
        }

        isect.geom.degenerated = false;
        isect.geom.infinite = true;

        // Tangent plane
        isect.geom.gn = -ray.d;
        isect.geom.sn = isect.geom.gn;
        isect.geom.ComputeTangentSpace();

        // Move the intersection point onto the virtual disk
        const auto p = ray.o + ray.d * t;
        const auto c = bound_.center + bound_.radius * ray.d;
        isect.geom.p = c + isect.geom.dpdu * Math::Dot(isect.geom.dpdu, p - c) + isect.geom.dpdv * Math::Dot(isect.geom.dpdv, p - c);

        // Primitive
        //isect.primitive = primitive_;

        return true;
    };

    //LM_IMPL_F(GetPrimitive) = [this]() -> const Primitive*
    //{
    //    return primitive_;
    //};

public:

    SphereBound bound_;
    //const Primitive* primitive_;

};

class Light_Directional final : public Light
{
public:

    LM_IMPL_CLASS(Light_Directional, Light);

public:

    LM_IMPL_F(Load) = [this](const PropertyNode* prop, Assets* assets, const Primitive* primitive) -> bool
    {
        primitive_ = primitive;
        Le_ = SPD::FromRGB(prop->ChildAs<Vec3>("Le", Vec3()));
        direction_ = Mat3(primitive->transform) * Math::Normalize(prop->ChildAs<Vec3>("direction", Vec3()));
        return true;
    };

    LM_IMPL_F(PostLoad) = [this](const Scene* scene_) -> bool
    {
        const auto* scene = static_cast<const Scene3*>(scene_);
        bound_ = scene->GetSphereBound();
        invArea_ = 1_f / (Math::Pi() * bound_.radius * bound_.radius);
        emitterShape_.reset(new EmitterShape_DirectionalLight(bound_, primitive_));
        return true;
    };

public:

    LM_IMPL_F(Type) = [this]() -> int
    {
        return SurfaceInteractionType::L;
    };

    LM_IMPL_F(SamplePositionGivenPreviousPosition) = [this](const Vec2& u, const SurfaceGeometry& geomPrev, SurfaceGeometry& geom) -> void
    {
        // Calculate intersection point on virtual disk
        Ray ray = { geomPrev.p, -direction_ };
        Intersection isect;
        if (!emitterShape_->Intersect(ray, 0_f, Math::Inf(), isect))
        {
            LM_UNREACHABLE();
            return;
        }

        // Sampled surface geometry
        geom = isect.geom;
    };

    LM_IMPL_F(SamplePositionAndDirection) = [this](const Vec2& u, const Vec2& u2, SurfaceGeometry& geom, Vec3& wo) -> void
    {
        // Sample a point on the virtual disk
        const auto p = Sampler::UniformConcentricDiskSample(u2) * bound_.radius;

        // Sampled surface geometry
        geom.degenerated = false;
        geom.infinite = true;
        geom.gn = direction_;
        geom.sn = geom.gn;
        geom.ComputeTangentSpace();
        geom.p = bound_.center - direction_ * bound_.radius + (geom.dpdu * p.x + geom.dpdv * p.y);

        // Sampled direction
        wo = direction_;
    };

    LM_IMPL_F(EvaluateDirectionPDF) = [this](const SurfaceGeometry& geom, int queryType, const Vec3& wi, const Vec3& wo, bool evalDelta) -> PDFVal
    {
        return PDFVal(PDFMeasure::ProjectedSolidAngle, evalDelta ? 0_f : 1_f);
    };

    LM_IMPL_F(EvaluatePositionGivenDirectionPDF) = [this](const SurfaceGeometry& geom, const Vec3& wo, bool evalDelta) -> PDFVal
    {
        return PDFVal(PDFMeasure::Area, invArea_);
    };

    LM_IMPL_F(EvaluatePositionGivenPreviousPositionPDF) = [this](const SurfaceGeometry& geom, const SurfaceGeometry& geomPrev, bool evalDelta) -> PDFVal
    {
        if (evalDelta) { return PDFVal(PDFMeasure::Area, 0_f); }
        return PDFVal(PDFMeasure::SolidAngle, 1_f).ConvertToArea(geomPrev, geom);
    };

    LM_IMPL_F(EvaluateDirection) = [this](const SurfaceGeometry& geom, int types, const Vec3& wi, const Vec3& wo, TransportDirection transDir, bool evalDelta) -> SPD
    {
        return evalDelta ? SPD() : Le_;
    };

    LM_IMPL_F(EvaluatePosition) = [this](const SurfaceGeometry& geom, bool evalDelta) -> SPD
    {
        return SPD(1_f);
    };

    LM_IMPL_F(IsDeltaDirection) = [this](int type) -> bool
    {
        return true;
    };

    LM_IMPL_F(IsDeltaPosition) = [this](int type) -> bool
    {
        return false;
    };

public:

    SPD Le_;
    Vec3 direction_;
    SphereBound bound_;
    Float invArea_;
    const Primitive* primitive_;
    std::unique_ptr<EmitterShape_DirectionalLight> emitterShape_;

};

LM_COMPONENT_REGISTER_IMPL(Light_Directional, "light::directional");

LM_NAMESPACE_END
