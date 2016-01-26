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

LM_NAMESPACE_BEGIN

class Light_Area final : public Light
{
public:

    LM_IMPL_CLASS(Light_Area, Light);

public:

    LM_IMPL_F(Load) = [this](const PropertyNode* prop, Assets* assets, const Primitive* primitive) -> bool
    {
        #pragma region Load parameters

        Le_ = SPD::FromRGB(prop->ChildAs<Vec3>("Le", Vec3()));

        #pragma endregion

        // --------------------------------------------------------------------------------

        #pragma region Create distribution according to triangle area

        // Function to create discrete distribution for sampling area light or raw sensor
        const auto CreateTriangleAreaDist = [](const Primitive* primitive, Distribution1D& dist, Float& invArea)
        {
            assert(primitive->mesh);
            Float sumArea = 0;
            dist.Clear();
            const auto* fs = primitive->mesh->Faces();
            const auto* ps = primitive->mesh->Positions();
            for (size_t i = 0; i < primitive->mesh->NumFaces(); i++)
            {
                unsigned int i1 = fs[3 * i];
                unsigned int i2 = fs[3 * i + 1];
                unsigned int i3 = fs[3 * i + 2];
                Vec3 p1(primitive->transform * Vec4(ps[3 * i1], ps[3 * i1 + 1], ps[3 * i1 + 2], 1_f));
                Vec3 p2(primitive->transform * Vec4(ps[3 * i2], ps[3 * i2 + 1], ps[3 * i2 + 2], 1_f));
                Vec3 p3(primitive->transform * Vec4(ps[3 * i3], ps[3 * i3 + 1], ps[3 * i3 + 2], 1_f));
                const Float area = Math::Length(Math::Cross(p2 - p1, p3 - p1)) * 0.5_f;
                dist.Add(area);
                sumArea += area;
            }
            dist.Normalize();
            invArea = 1_f / sumArea;
        };

        primitive_ = primitive;
        CreateTriangleAreaDist(primitive_, dist_, invArea_);

        #pragma endregion

        // --------------------------------------------------------------------------------

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
        // Function to sample a position on the triangle mesh
        const auto SampleTriangleMesh = [](const Vec2& u, const Primitive* primitive, const Distribution1D& dist, SurfaceGeometry& geom)
        {
            #pragma region Sample a triangle & a position on triangle

            auto u2 = u;
            const int i = dist.SampleReuse(u.x, u2.x);
            const auto b = Sampler::UniformSampleTriangle(u2);

            #pragma endregion

            // --------------------------------------------------------------------------------

            #pragma region Store surface geometry information

            const auto* mesh = primitive->mesh;
            const auto* fs = mesh->Faces();
            const auto* ps = mesh->Positions();
            unsigned int i1 = fs[3 * i];
            unsigned int i2 = fs[3 * i + 1];
            unsigned int i3 = fs[3 * i + 2];

            // Position
            Vec3 p1(primitive->transform * Vec4(ps[3 * i1], ps[3 * i1 + 1], ps[3 * i1 + 2], 1_f));
            Vec3 p2(primitive->transform * Vec4(ps[3 * i2], ps[3 * i2 + 1], ps[3 * i2 + 2], 1_f));
            Vec3 p3(primitive->transform * Vec4(ps[3 * i3], ps[3 * i3 + 1], ps[3 * i3 + 2], 1_f));
            geom.p = p1 * (1_f - b.x - b.y) + p2 * b.x + p3 * b.y;

            // UV
            const auto* tc = mesh->Texcoords();
            if (!tc)
            {
                Vec2 uv1(tc[2 * i1], tc[2 * i1 + 1]);
                Vec2 uv2(tc[2 * i2], tc[2 * i2 + 1]);
                Vec2 uv3(tc[2 * i3], tc[2 * i3 + 1]);
                geom.uv = uv1 * (1_f - b.x - b.y) + uv2 * b.x + uv3 * b.y;
            }

            // Normal
            geom.degenerated = false;
            geom.gn = Math::Normalize(Math::Cross(p2 - p1, p3 - p1));
            geom.sn = geom.gn;

            // Compute tangent space
            Math::OrthonormalBasis(geom.sn, geom.dpdu, geom.dpdv);
            geom.ToWorld = Mat3(geom.dpdu, geom.dpdv, geom.sn);
            geom.ToLocal = Math::Transpose(geom.ToWorld);

            // Normal derivatives
            geom.dndu = geom.dndv = Vec3();

            #pragma endregion
        };

        SampleTriangleMesh(u, primitive_, dist_, geom);
    };

    LM_IMPL_F(EvaluatePositionPDF) = [this](const SurfaceGeometry& geom, bool evalDelta) -> Float
    {
        return invArea_;
    };

    LM_IMPL_F(EvaluatePosition) = [this](const SurfaceGeometry& geom, bool evalDelta) -> SPD
    {
        return SPD(1_f);
    };

    LM_IMPL_F(RasterPosition) = [this](const Vec3& wo, const SurfaceGeometry& geom, Vec2& rasterPos) -> bool
    {
        return false;
    };

private:

    SPD Le_;
    Distribution1D dist_;
    Float invArea_;
    const Primitive* primitive_;

};

LM_COMPONENT_REGISTER_IMPL(Light_Area, "light::area");

LM_NAMESPACE_END
