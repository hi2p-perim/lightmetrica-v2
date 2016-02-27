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
#include <lightmetrica/scene.h>

LM_NAMESPACE_BEGIN

class EmitterShape_EnvLight final : public EmitterShape
{
public:

    LM_IMPL_CLASS(EmitterShape_EnvLight, EmitterShape);

public:

    EmitterShape_EnvLight(const SphereBound& bound, const Primitive* primitive)
        : bound_(bound)
        , primitive_(primitive)
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
        const auto p  = ray.o + ray.d * t;
        const auto c  = bound_.center + bound_.radius * ray.d;
        isect.geom.p = c + isect.geom.dpdu * Math::Dot(isect.geom.dpdu, p - c) + isect.geom.dpdv * Math::Dot(isect.geom.dpdv, p - c);

        // Primitive
        isect.primitive = primitive_;

        return true;
    };

    LM_IMPL_F(GetPrimitive) = [this]() -> const Primitive*
    {
        return primitive_;
    };

public:

    SphereBound bound_;
    const Primitive* primitive_;

};

class Light_EnvLight final : public Light
{
public:

    LM_IMPL_CLASS(Light_EnvLight, Light);

public:

    LM_IMPL_F(Load) = [this](const PropertyNode* prop, Assets* assets, const Primitive* primitive) -> bool
    {
        primitive_ = primitive;
        return true;
    };

    LM_IMPL_F(PostLoad) = [this](const Scene* scene) -> bool
    {
        bound_ = scene->GetSphereBound();
        invArea_ = 1_f / (2_f * Math::Pi() * bound_.radius * bound_.radius);
        emitterShape_.reset(new EmitterShape_EnvLight(bound_, primitive_));
        return true;
    };

public:

    LM_IMPL_F(Type) = [this]() -> int
    {
        return SurfaceInteraction::L;
    };

    LM_IMPL_F(SampleDirection) = [this](const Vec2& u, Float uComp, int queryType, const SurfaceGeometry& geom, const Vec3& wi, Vec3& wo) -> void
    {
        wo = geom.gn;
    };

    LM_IMPL_F(EvaluateDirectionPDF) = [this](const SurfaceGeometry& geom, int queryType, const Vec3& wi, const Vec3& wo, bool evalDelta) -> Float
    {
        return 1_f;
    };

    LM_IMPL_F(EvaluateDirection) = [this](const SurfaceGeometry& geom, int types, const Vec3& wi, const Vec3& wo, TransportDirection transDir, bool evalDelta) -> SPD
    {
        // TODO
        return SPD(1_f);
    };

public:

    LM_IMPL_F(SamplePosition) = [this](const Vec2& u, const Vec2& u2, SurfaceGeometry& geom) -> void
    {
        // Sample a direction from p_\omega(wo)
        // Here we sample from the uniform sphere
        const auto d = Sampler::UniformSampleSphere(u);

        // Sample a point on the virtual disk
        const auto p = Sampler::UniformConcentricDiskSample(u2) * bound_.radius;

        // Surface geometry
        geom.degenerated = false;
        geom.infinite = true;
        geom.gn = -d;
        geom.sn = geom.gn;
        geom.ComputeTangentSpace();
        geom.p = center_ + d * bound_.radius + (geom.dpdu * p.x + geom.dpdv * p.y);
    };

    LM_IMPL_F(EvaluatePositionPDF) = [this](const SurfaceGeometry& geom, bool evalDelta) -> Float
    {
        return Sampler::UniformSampleSpherePDFSA() * invArea_;
    };

    LM_IMPL_F(EvaluatePosition) = [this](const SurfaceGeometry& geom, bool evalDelta) -> SPD
    {
        return SPD(1);
    };

    LM_IMPL_F(GetEmitterShape) = [this]() -> const EmitterShape*
    {  
        return emitterShape_.get();
    };

public:

    Vec3 center_;
    SphereBound bound_;
    Float invArea_;
    const Primitive* primitive_;
    std::unique_ptr<EmitterShape_EnvLight> emitterShape_;

};

LM_COMPONENT_REGISTER_IMPL(Light_EnvLight, "light::env");

LM_NAMESPACE_END
 