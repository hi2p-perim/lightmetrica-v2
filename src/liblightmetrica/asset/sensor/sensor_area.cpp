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
#include <lightmetrica/sensor.h>
#include <lightmetrica/property.h>
#include <lightmetrica/trianglemesh.h>
#include <lightmetrica/dist.h>
#include <lightmetrica/primitive.h>
#include <lightmetrica/sampler.h>
#include <lightmetrica/surfacegeometry.h>
#include <lightmetrica/triangleutils.h>
#include <lightmetrica/assets.h>
#include <lightmetrica/film.h>

LM_NAMESPACE_BEGIN

class Sensor_Area final : public Sensor
{
public:

    LM_IMPL_CLASS(Sensor_Area, Sensor);

public:

    LM_IMPL_F(Load) = [this](const PropertyNode* prop, Assets* assets, const Primitive* primitive) -> bool
    {
        // Load parameters
        We_ = SPD::FromRGB(prop->ChildAs<Vec3>("We", Vec3()));

        // Film
        const auto filmID = prop->Child("film")->As<std::string>();
        film_ = static_cast<Film*>(assets->AssetByIDAndType(filmID, "film", primitive));

        // Create distribution according to triangle area
        primitive_ = primitive;
        TriangleUtils::CreateTriangleAreaDist(primitive_, dist_, invArea_);

        return true;
    };

    LM_IMPL_F(GetFilm) = [this]() -> Film*
    {
        return film_;
    };

public:

    LM_IMPL_F(Type) = [this]() -> int
    {
        return SurfaceInteraction::E;
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
        return We_;
    };

public:

    LM_IMPL_F(SamplePosition) = [this](const Vec2& u, const Vec2& u2, SurfaceGeometry& geom) -> void
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

    LM_IMPL_F(RasterPosition) = [this](const Vec3& wo, const SurfaceGeometry& geom, Vec2& rasterPos) -> bool
    {
        rasterPos = geom.uv;
        return true;
    };

private:

    SPD We_;
    Distribution1D dist_;
    Float invArea_;
    const Primitive* primitive_;
    Film* film_;

};

LM_COMPONENT_REGISTER_IMPL(Sensor_Area, "sensor::area");

LM_NAMESPACE_END
