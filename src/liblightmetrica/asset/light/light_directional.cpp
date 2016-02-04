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

class Light_Directional final : public Light
{
public:

    LM_IMPL_CLASS(Light_Directional, Light);

public:

    LM_IMPL_F(Load) = [this](const PropertyNode* prop, Assets* assets, const Primitive* primitive) -> bool
    {
        Le_ = SPD::FromRGB(prop->ChildAs<Vec3>("Le", Vec3()));
        const auto d = prop->ChildAs<Vec3>("direction", Vec3());
        direction_ = Mat3(primitive->transform) * d;
        return true;
    };

    LM_IMPL_F(PostLoad) = [this](const Scene* scene) -> bool
    {
        const auto bound = scene->GetBound();
        center_  = (bound.max + bound.min) * .5_f;
        radius_  = Math::Length(center_ - bound.max) * 1.01_f;  // Grow slightly
        invArea_ = 1_f / (2_f * Math::Pi() * radius_ * radius_);
        return true;
    };

public:

    LM_IMPL_F(Type) = [this]() -> int
    {
        return SurfaceInteraction::L;
    };

    LM_IMPL_F(SampleDirection) = [this](const Vec2& u, Float uComp, int queryType, const SurfaceGeometry& geom, const Vec3& wi, Vec3& wo) -> void
    {
        wo = direction_;
    };

    LM_IMPL_F(EvaluateDirectionPDF) = [this](const SurfaceGeometry& geom, int queryType, const Vec3& wi, const Vec3& wo, bool evalDelta) -> Float
    {
        return evalDelta ? 0_f : 1_f;
    };

    LM_IMPL_F(EvaluateDirection) = [this](const SurfaceGeometry& geom, int types, const Vec3& wi, const Vec3& wo, TransportDirection transDir, bool evalDelta) -> SPD
    {
        return evalDelta ? SPD() : Le_;
    };

public:

    LM_IMPL_F(SamplePosition) = [this](const Vec2& u, SurfaceGeometry& geom) -> void
    {
        // Sample a point on the virtual disk
        const auto p = Sampler::UniformConcentricDiskSample(u) * radius_;

        // Normals
        geom.gn = direction_;
        geom.sn = geom.gn;

        // Compute tangent space
        Math::OrthonormalBasis(geom.sn, geom.dpdu, geom.dpdv);
        geom.ToWorld = Mat3(geom.dpdu, geom.dpdv, geom.sn);
        geom.ToLocal = Math::Transpose(geom.ToWorld);

        // Position
        geom.degenerated = false;
        geom.p = center_ - direction_ * radius_ + (geom.dpdu * p.x + geom.dpdv * p.y);
    };

    LM_IMPL_F(EvaluatePositionPDF) = [this](const SurfaceGeometry& geom, bool evalDelta) -> Float
    {
        return invArea_;
    };

    LM_IMPL_F(EvaluatePosition) = [this](const SurfaceGeometry& geom, bool evalDelta) -> SPD
    {
        return SPD(1);
    };

public:

    SPD Le_;
    Vec3 direction_;
    Vec3 center_;
    Float radius_;
    Float invArea_;

};

LM_COMPONENT_REGISTER_IMPL(Light_Directional, "light::directional");

LM_NAMESPACE_END
