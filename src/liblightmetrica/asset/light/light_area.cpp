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

LM_NAMESPACE_BEGIN

class Light_Area final : public Light
{
public:

    LM_IMPL_CLASS(Light_Area, Light);

public:

    LM_IMPL_F(Load) = [this](const PropertyNode* prop, Assets* assets, const Primitive* primitive) -> bool
    {
        // Load parameters
        Le_ = SPD::FromRGB(prop->ChildAs<Vec3>("Le", Vec3()));

        // Create distribution according to triangle area
        primitive_ = primitive;
        TriangleUtils::CreateTriangleAreaDist(primitive_, dist_, invArea_);

        return true;
    };

    LM_IMPL_F(PostLoad) = [this](const Scene* scene) -> bool
    {
        return true;
    };

public:

    LM_IMPL_F(Type) = [this]() -> int
    {
        return SurfaceInteraction::L;
    };

    LM_IMPL_F(SampleDirection) = [this](const Vec2& u, Float uComp, int queryType, const SurfaceGeometry& geom, const Vec3& wi, Vec3& wo) -> void
    {
        const auto localWo = Sampler::CosineSampleHemisphere(u);
        wo = geom.ToWorld * localWo;
    };

    LM_IMPL_F(EvaluateDirectionPDF) = [this](const SurfaceGeometry& geom, int queryType, const Vec3& wi, const Vec3& wo, bool evalDelta) -> Float
    {
        const auto localWo = geom.ToLocal * wo;
        if (Math::LocalCos(localWo) <= 0_f) { return 0_f; }
        return Sampler::CosineSampleHemispherePDFProjSA(localWo);
    };

    LM_IMPL_F(EvaluateDirection) = [this](const SurfaceGeometry& geom, int types, const Vec3& wi, const Vec3& wo, TransportDirection transDir, bool evalDelta) -> SPD
    {
        const auto localWo = geom.ToLocal * wo;
        if (Math::LocalCos(localWo) <= 0) { return SPD(); }
        return Le_;
    };

public:

    LM_IMPL_F(SamplePosition) = [this](const Vec2& u, SurfaceGeometry& geom) -> void
    {
        TriangleUtils::SampleTriangleMesh(u, primitive_, dist_, geom);
    };

    LM_IMPL_F(EvaluatePositionPDF) = [this](const SurfaceGeometry& geom, bool evalDelta) -> Float
    {
        return invArea_;
    };

    LM_IMPL_F(EvaluatePosition) = [this](const SurfaceGeometry& geom, bool evalDelta) -> SPD
    {
        return SPD(1_f);
    };

private:

    SPD Le_;
    Distribution1D dist_;
    Float invArea_;
    const Primitive* primitive_;

};

LM_COMPONENT_REGISTER_IMPL(Light_Area, "light::area");

LM_NAMESPACE_END
