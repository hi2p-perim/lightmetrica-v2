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
#include <lightmetrica/renderutils.h>
#include <lightmetrica/texture.h>
#include <lightmetrica/assets.h>
#include <lightmetrica/detail/serial.h>

LM_NAMESPACE_BEGIN

class EmitterShape_EnvLight final : public EmitterShape
{
public:

    LM_IMPL_CLASS(EmitterShape_EnvLight, EmitterShape);

public:

    EmitterShape_EnvLight(const SphereBound& bound)
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
        const auto p  = ray.o + ray.d * t;
        const auto c  = bound_.center + bound_.radius * ray.d;
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

class Light_EnvLight final : public Light
{
public:

    LM_IMPL_CLASS(Light_EnvLight, Light);

public:

    LM_IMPL_F(Load) = [this](const PropertyNode* prop, Assets* assets, const Primitive* primitive) -> bool
    {
        if (prop->Child("envmap"))
        {
            std::string id;
            prop->ChildAs("envmap", id);
            envmap_ = static_cast<const Texture*>(assets->AssetByIDAndType(id, "texture", primitive));
            if (envmap_) return false;
        }
        else
        {
            Le_ = SPD::FromRGB(prop->ChildAs<Vec3>("Le", Vec3(1_f)));
        }

        rotate_ = prop->ChildAs<Float>("rotate", 0_f);

        return true;
    };

    LM_IMPL_F(PostLoad) = [this](const Scene* scene_) -> bool
    {
        const auto* scene = static_cast<const Scene3*>(scene_);
        bound_ = scene->GetSphereBound();
        invArea_ = 1_f / (Math::Pi() * bound_.radius * bound_.radius);
        emitterShape_.reset(new EmitterShape_EnvLight(bound_));
        return true;
    };

public:

    LM_IMPL_F(Type) = [this]() -> int
    {
        return SurfaceInteractionType::L;
    };

    // Sample x \sim p_A(x | x_prev)
    LM_IMPL_F(SamplePositionGivenPreviousPosition) = [this](const Vec2& u, const SurfaceGeometry& geomPrev, SurfaceGeometry& geom) -> void
    {
        // First sample a direction from p_\omega(wo)
        const auto d = Sampler::UniformSampleSphere(u);

        // Calculate intersection point on virtual disk
        Ray ray = { geomPrev.p, d };
        Intersection isect;
        if (!emitterShape_->Intersect(ray, 0_f, Math::Inf(), isect))
        {
            LM_UNREACHABLE();
            return;
        }
        
        // Sampled surface geometry
        geom = isect.geom;
    };

    // Sample (x, \omega) \sim p_{A,\sigma^\perp}(x, \omega_o) = p_{\sigma^\perp}(\omega_o) p_A(x | \omega_o)
    LM_IMPL_F(SamplePositionAndDirection) = [this](const Vec2& u, const Vec2& u2, SurfaceGeometry& geom, Vec3& wo) -> void
    {
        // Sample a direction from p_\omega(wo)
        const auto d = Sampler::UniformSampleSphere(u);

        // Sample a point on the virtual disk
        const auto p = Sampler::UniformConcentricDiskSample(u2) * bound_.radius;

        // Sampled surface geometry
        geom.degenerated = false;
        geom.infinite = true;
        geom.gn = -d;
        geom.sn = geom.gn;
        geom.ComputeTangentSpace();
        geom.p = bound_.center + d * bound_.radius + (geom.dpdu * p.x + geom.dpdv * p.y);

        // Sampled direction
        wo = -d;
    };

    // Evaluate p_{\sigma^\perp}(\omega_o)
    LM_IMPL_F(EvaluateDirectionPDF) = [this](const SurfaceGeometry& geom, int queryType, const Vec3& wi, const Vec3& wo, bool evalDelta) -> PDFVal
    {
        // |cos(geom.sn, wo)| is always 1
        return PDFVal(PDFMeasure::ProjectedSolidAngle, Sampler::UniformSampleSpherePDFSA().v);
    };

    // Evaluate p_A(x | \omega_o)
    LM_IMPL_F(EvaluatePositionGivenDirectionPDF) = [this](const SurfaceGeometry& geom, const Vec3& wo, bool evalDelta) -> PDFVal
    {
        return PDFVal(PDFMeasure::Area, invArea_);
    };

    // Evaluate p_A(x | x_prev)
    LM_IMPL_F(EvaluatePositionGivenPreviousPositionPDF) = [this](const SurfaceGeometry& geom, const SurfaceGeometry& geomPrev, bool evalDelta) -> PDFVal
    {
        if (evalDelta) { return PDFVal(PDFMeasure::Area, 0_f); }
        return Sampler::UniformSampleSpherePDFSA().ConvertToArea(geomPrev, geom);
    };

    LM_IMPL_F(EvaluateDirection) = [this](const SurfaceGeometry& geom, int types, const Vec3& wi, const Vec3& wo, TransportDirection transDir, bool evalDelta) -> SPD
    {
        if (evalDelta)
        {
            return 0_f;
        }

        if (envmap_)
        {
            // Convert ray direction to the uv coordinates of light probe
            // See http://www.pauldebevec.com/Probes/ for details
            const auto d = -Vec3(Math::Rotate(Math::Radians(rotate_), Vec3(0_f, 1_f, 0_f)) * Vec4(wo.x, wo.y, wo.z, 0_f));
            const auto r = (1_f / Math::Pi()) * Math::Acos(Math::Clamp(d.z, -1_f, 1_f)) / Math::Sqrt(d.x*d.x + d.y*d.y);
            const auto uv = (Vec2(d.x, -d.y) * r + Vec2(1_f)) * .5_f;
            return SPD::FromRGB(envmap_->Evaluate(uv));
        }

        return Le_;
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

    LM_IMPL_F(GetEmitterShape) = [this]() -> const EmitterShape*
    {  
        return emitterShape_.get();
    };

    LM_IMPL_F(Serialize) = [this](std::ostream& stream) -> bool
    {
        {
            cereal::PortableBinaryOutputArchive oa(stream);
            int envmapID = envmap_ ? envmap_->Index() : -1;
            oa(bound_, invArea_, Le_, envmapID, rotate_);
        }
        return true;
    };

    LM_IMPL_F(Deserialize) = [this](std::istream& stream, const std::unordered_map<std::string, void*>& userdata) -> bool
    {
        int envmapID;
        {
            cereal::PortableBinaryInputArchive ia(stream);
            ia(bound_, invArea_, Le_, envmapID, rotate_);
        }
        if (envmapID >= 0)
        {
            auto* assets = static_cast<Assets*>(userdata.at("assets"));
            envmap_ = static_cast<const Texture*>(assets->GetByIndex(envmapID));
        }
        return true;
    };

public:

    SphereBound bound_;
    Float invArea_;
    std::unique_ptr<EmitterShape_EnvLight> emitterShape_;

    SPD Le_;
    const Texture* envmap_ = nullptr;
    Float rotate_;

};

LM_COMPONENT_REGISTER_IMPL(Light_EnvLight, "light::env");

LM_NAMESPACE_END
 