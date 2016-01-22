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

#include <lightmetrica/sensor.h>
#include <lightmetrica/property.h>
#include <lightmetrica/primitive.h>
#include <lightmetrica/assets.h>
#include <lightmetrica/film.h>
#include <lightmetrica/surfacegeometry.h>

LM_NAMESPACE_BEGIN

class Sensor_Pinhole : public Sensor
{
public:

    LM_IMPL_CLASS(Sensor_Pinhole, Sensor);

public:

    LM_IMPL_F(Load) = [this](const PropertyNode* prop, Assets* assets, Primitive* primitive) -> bool
    {
        // Load parameters
        We_  = prop->Child("We")->As<Vec3>();
        fov_ = Math::Radians(prop->Child("fov")->As<Float>());

        // Camera vectors
        const auto eye    = prop->Child("eye")->As<Vec3>();
        const auto center = prop->Child("center")->As<Vec3>();
        const auto up     = prop->Child("up")->As<Vec3>();
        position_ = eye;
        vz_ = Math::Normalize(eye - center);
        vx_ = Math::Normalize(Math::Cross(up, vz_));
        vy_ = Math::Cross(vz_, vx_);

        // Aspect ratio
        const auto filmID = prop->Child("film")->As<std::string>();
        film_ = assets->AssetByID<Film>(filmID, primitive);
        aspect_ = (Float)(film_->Width()) / (Float)(film_->Height());

        return true;
    };

    LM_IMPL_F(GetFilm) = [this]() -> const Film*
    {
        return film_;
    };


    LM_IMPL_F(RasterPosition) = [this](const Vec3& wo, const SurfaceGeometry& geom, Vec2& rasterPos) -> bool
    {
        // Check if wo is coming from bind the camera
        const auto V = Math::Transpose(Math::Mat3(vx_, vy_, vz_));
        const auto woEye = V * wo;
        if (LocalCos(woEye) >= 0_f)
        {
            return false;
        }

        // Calculate raster position
        // Check if #wo is outside of the screen
        const double tanFov = Math::Tan(fov_ * 0.5_f);
        rasterPos = (Vec2(-woEye.x / woEye.z / tanFov / aspect_, -woEye.y / woEye.z / tanFov) + Vec2(1_f)) * 0.5_f;
        if (rasterPos.x < 0_f || rasterPos.x > 1_f || rasterPos.y < 0_f || rasterPos.y > 1_f)
        {
            return false;
        }

        return true;
    };

public:

    #pragma region GeneralizedBSDF

    LM_IMPL_F(Sample) = [this](const Vec2& u, double uComp, int queryType, const SurfaceGeometry& geom, const Vec3& wi, Vec3& wo) -> void
    {
        const auto rasterPos = 2.0_f * u - 1.0_f;
        const double tanFov = Math::Tan(fov_ * 0.5_f);
        const auto woEye = Math::Normalize(Vec3(aspect_ * tanFov * rasterPos.x, tanFov * rasterPos.y, -1_f));
        wo = vx_ * woEye.x + vy_ * woEye.y + vz_ * woEye.z;
    };

    LM_IMPL_F(EvaluatePDF) = [this](const SurfaceGeometry& geom, int queryType, const Vec3& wi, const Vec3& wo, bool evalDelta) -> Float
    {
        // Calculate raster position
        glm::dvec2 rasterPos;
        if (!RasterPosition(wo, geom, rasterPos))
        {
            return 0;
        }

        // Evaluate importance
        const auto V = glm::transpose(glm::dmat3(Params.E.Pinhole.Vx, Params.E.Pinhole.Vy, Params.E.Pinhole.Vz));
        const auto woEye = V * wo;
        const double tanFov = glm::tan(Params.E.Pinhole.Fov * 0.5);
        const double cosTheta = -LocalCos(woEye);
        const double invCosTheta = 1.0 / cosTheta;
        const double A = tanFov * tanFov * Params.E.Pinhole.Aspect * 4.0;
        return invCosTheta * invCosTheta * invCosTheta / A;
    };

    LM_IMPL_F(EvaluateDirection) = [this](const SurfaceGeometry& geom, int types, const Vec3& wi, const Vec3& wo, TransportDirection transDir, bool evalDelta) -> SPD
    {
        // Calculate raster position
        glm::dvec2 rasterPos;
        if (!RasterPosition(wo, geom, rasterPos))
        {
            return glm::dvec3();
        }

        // Evaluate importance
        const auto V = glm::transpose(glm::dmat3(Params.E.Pinhole.Vx, Params.E.Pinhole.Vy, Params.E.Pinhole.Vz));
        const auto woEye = V * wo;
        const double tanFov = glm::tan(Params.E.Pinhole.Fov * 0.5);
        const double cosTheta = -LocalCos(woEye);
        const double invCosTheta = 1.0 / cosTheta;
        const double A = tanFov * tanFov * Params.E.Pinhole.Aspect * 4.0;
        return glm::dvec3(invCosTheta * invCosTheta * invCosTheta / A);
    };

    #pragma endregion

public:

    #pragma region Emitter

    LM_IMPL_F(SamplePosition) = [this](const Vec2& u, SurfaceGeometry& geom) -> void
    {
        geom.degenerated = true;
        geom.p = position_;
    };

    LM_IMPL_F(EvaluatePositionPDF) = [this](const SurfaceGeometry& geom, bool evalDelta) -> Float
    {
        return !evalDelta ? 1 : 0;
    };

    LM_IMPL_F(EvaluatePosition) = [this](const SurfaceGeometry& geom, bool evalDelta) -> SPD
    {
        return !evalDelta ? SPD(1) : SPD();
    };
    
    #pragma endregion

private:

    Vec3 We_;
    Float fov_;
    Vec3 position_;
    Vec3 vx_;
    Vec3 vy_;
    Vec3 vz_;
    const Film* film_;
    Float aspect_;

};

LM_COMPONENT_REGISTER_IMPL(Sensor_Pinhole, "pinhole");

LM_NAMESPACE_END
