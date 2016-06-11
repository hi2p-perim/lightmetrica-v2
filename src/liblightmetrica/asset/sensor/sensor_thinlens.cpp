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
#include <lightmetrica/primitive.h>
#include <lightmetrica/assets.h>
#include <lightmetrica/film.h>
#include <lightmetrica/surfacegeometry.h>
#include <lightmetrica/sampler.h>

LM_NAMESPACE_BEGIN

class Sensor_ThinLens final : public Sensor
{
public:

    LM_IMPL_CLASS(Sensor_ThinLens, Sensor);

public:

    LM_IMPL_F(Load) = [this](const PropertyNode* prop, Assets* assets, const Primitive* primitive) -> bool
    {
        // Load parameters
        We_  = prop->ChildAs<Vec3>("We", Vec3(1_f));
        fov_ = Math::Radians(prop->ChildAs("fov", 45_f));

        // Position & eye coordianates
        position_ = Vec3(primitive->transform * Vec4(0_f, 0_f, 0_f, 1_f));
        vx_ = Vec3(primitive->transform[0]);
        vy_ = Vec3(primitive->transform[1]);
        vz_ = Vec3(primitive->transform[2]);

        // Film & aspect ratio
        std::string filmid;
        if (!prop->ChildAs("film", filmid)) return false;
        film_ = static_cast<Film*>(assets->AssetByIDAndType(filmid, "film", primitive));
        if (film_ == nullptr) return false;
        aspect_ = (Float)(film_->Width()) / (Float)(film_->Height());

        // Aperture radius & focal distance
        lensRadius_ = prop->ChildAs<Float>("lens_radius", 0.1_f);
        focalDistance_  = prop->ChildAs<Float>("focal_distance", 1_f);

        return true;
    };

public:

    LM_IMPL_F(Type) = [this]() -> int
    {
        return SurfaceInteractionType::E;
    };

    LM_IMPL_F(SamplePositionGivenPreviousPosition) = [this](const Vec2& u, const SurfaceGeometry& geomPrev, SurfaceGeometry& geom) -> void
    {
        // Handle the point on the lens as spatially degenerated position
        // the bidirectional handling of the position becomes same as pinhole camera.
        geom.degenerated = true;
        const auto lensUV = Sampler::UniformConcentricDiskSample(u) * lensRadius_;
        geom.p = position_ + lensUV.x * vx_ + lensUV.y * vy_;
    };

    LM_IMPL_F(SamplePositionAndDirection) = [this](const Vec2& u, const Vec2& u2, SurfaceGeometry& geom, Vec3& wo) -> void
    {
        // Sample a position on the lens
        geom.degenerated = true;
        const auto lensUV = Sampler::UniformConcentricDiskSample(u2) * lensRadius_;
        geom.p = position_ + lensUV.x * vx_ + lensUV.y * vy_;

        // Sample a direction
        const auto rasterPos = 2.0_f * u - Vec2(1.0_f);
        const Float tanFov = Math::Tan(fov_ * 0.5_f);
        const auto woEye = Math::Normalize(Vec3(aspect_ * tanFov * rasterPos.x, tanFov * rasterPos.y, -1_f));
        const auto rayDir = vx_ * woEye.x + vy_ * woEye.y + vz_ * woEye.z;

        // Compute the intersection point with the focal plane
        const float tf = focalDistance_ / Math::Dot(-vz_, rayDir);
        const auto Pf = position_ + rayDir * tf;

        // Final ray direction
        wo = Math::Normalize(Pf - geom.p);
    };

    LM_IMPL_F(EvaluateDirectionPDF) = [this](const SurfaceGeometry& geom, int queryType, const Vec3& wi, const Vec3& wo, bool evalDelta) -> PDFVal
    {
        return PDFVal(PDFMeasure::ProjectedSolidAngle, Importance(wo, geom));
    };

    LM_IMPL_F(EvaluatePositionGivenDirectionPDF) = [this](const SurfaceGeometry& geom, const Vec3& wo, bool evalDelta) -> PDFVal
    {
        return PDFVal(PDFMeasure::Area, !evalDelta ? 1_f : 0_f);
    };

    LM_IMPL_F(EvaluatePositionGivenPreviousPositionPDF) = [this](const SurfaceGeometry& geom, const SurfaceGeometry& geomPrev, bool evalDelta) -> PDFVal
    {
        return PDFVal(PDFMeasure::Area, !evalDelta ? 1_f : 0_f);
    };

    LM_IMPL_F(EvaluateDirection) = [this](const SurfaceGeometry& geom, int types, const Vec3& wi, const Vec3& wo, TransportDirection transDir, bool evalDelta) -> SPD
    {
        return SPD(Importance(wo, geom));
    };

    LM_IMPL_F(EvaluatePosition) = [this](const SurfaceGeometry& geom, bool evalDelta) -> SPD
    {
        return !evalDelta ? SPD(1) : SPD();
    };

    LM_IMPL_F(IsDeltaDirection) = [this](int type) -> bool
    {
        return false;
    };

    LM_IMPL_F(IsDeltaPosition) = [this](int type) -> bool
    {
        return true;
    };

private:

    auto Importance(const Vec3& wo, const SurfaceGeometry& geom) const -> Float
    {
        // Calculate raster position
        Vec2 rasterPos;
        if (!RasterPosition(wo, geom, rasterPos))
        {
            return 0_f;
        }

        // Intersection point with focal plane
        assert(Math::Dot(-vz_, wo) > 0);
        const float tf = focalDistance_ / Math::Dot(-vz_, wo);
        const auto Pf = geom.p + wo * tf;

        // Original ray direction before refraction
        const auto woOrig = Math::Normalize(Pf - position_);

        // Evaluate importance
        const auto V = Math::Transpose(Mat3(vx_, vy_, vz_));
        const auto woEye = V * woOrig;
        const Float tanFov = Math::Tan(fov_ * 0.5_f);
        const Float cosTheta = -Math::LocalCos(woEye);
        const Float invCosTheta = 1_f / cosTheta;
        const Float A = tanFov * tanFov * aspect_ * 4_f;
        return invCosTheta * invCosTheta * invCosTheta / A;
    }

public:

    LM_IMPL_F(GetBound) = [this]() -> Bound
    {
        return Math::Union(Bound(), position_);
    };

public:

    LM_IMPL_F(RasterPosition) = [this](const Vec3& wo, const SurfaceGeometry& geom, Vec2& rasterPos) -> bool
    {
        // Check if wo is in the forward direction
        if (Math::Dot(-vz_, wo) <= 0)
        {
            return false;
        }

        // Intersection point with focal plane
        const float tf = focalDistance_ / Math::Dot(-vz_, wo);
        const auto Pf = geom.p + wo * tf;

        // Original ray direction before refraction
        const auto woOrig = Math::Normalize(Pf - position_);

        // Check if woOrig is in the forward direction
        const auto V = Math::Transpose(Mat3(vx_, vy_, vz_));
        const auto woEye = V * woOrig;
        if (Math::LocalCos(woEye) >= 0_f)
        {
            return false;
        }

        // Calculate raster position
        // Check if #wo is outside of the screen
        const Float tanFov = Math::Tan(fov_ * 0.5_f);
        rasterPos = (Vec2(-woEye.x / woEye.z / tanFov / aspect_, -woEye.y / woEye.z / tanFov) + Vec2(1_f)) * 0.5_f;
        if (rasterPos.x < 0_f || rasterPos.x > 1_f || rasterPos.y < 0_f || rasterPos.y > 1_f)
        {
            return false;
        }

        return true;
    };

    LM_IMPL_F(GetFilm) = [this]() -> Film*
    {
        return film_;
    };

private:

    Vec3 We_;
    Float fov_;
    Vec3 position_;
    Vec3 vx_;
    Vec3 vy_;
    Vec3 vz_;
    Film* film_;
    Float aspect_;
    Float lensRadius_;
    Float focalDistance_;

};

LM_COMPONENT_REGISTER_IMPL(Sensor_ThinLens, "sensor::thinlens");

LM_NAMESPACE_END
