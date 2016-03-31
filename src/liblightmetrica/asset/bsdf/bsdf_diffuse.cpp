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
#include <lightmetrica/bsdf.h>
#include <lightmetrica/property.h>
#include <lightmetrica/spectrum.h>
#include <lightmetrica/surfacegeometry.h>
#include <lightmetrica/bsdfutils.h>
#include <lightmetrica/sampler.h>
#include <lightmetrica/texture.h>
#include <lightmetrica/assets.h>

LM_NAMESPACE_BEGIN

class BSDF_Diffuse final : public BSDF
{
public:

    LM_IMPL_CLASS(BSDF_Diffuse, BSDF);

public:

    LM_IMPL_F(Load) = [this](const PropertyNode* prop, Assets* assets, const Primitive* primitive) -> bool
    {
        if (prop->Child("TexR"))
        {
            const auto id = prop->Child("TexR")->As<std::string>();
            texR_  = static_cast<const Texture*>(assets->AssetByIDAndType(id, "texture", primitive));
        }
        else
        {
            R_ = SPD::FromRGB(prop->ChildAs<Vec3>("R", Vec3()));
        }

        return true;
    };

public:

    LM_IMPL_F(Type) = [this]() -> int
    {
        return SurfaceInteractionType::D;
    };

    LM_IMPL_F(SampleDirection) = [this](const Vec2& u, Float uComp, int queryType, const SurfaceGeometry& geom, const Vec3& wi, Vec3& wo) -> void
    {
        const auto localWi = geom.ToLocal * wi;
        if (Math::LocalCos(localWi) <= 0_f)
        {
            return;
        }

        const auto localWo = Sampler::CosineSampleHemisphere(u);
        wo = geom.ToWorld * localWo;
    };

    LM_IMPL_F(EvaluateDirectionPDF) = [this](const SurfaceGeometry& geom, int queryType, const Vec3& wi, const Vec3& wo, bool evalDelta) -> PDFVal
    {
        const auto localWi = geom.ToLocal * wi;
        const auto localWo = geom.ToLocal * wo;
        if (Math::LocalCos(localWi) <= 0_f || Math::LocalCos(localWo) <= 0_f)
        {
            return PDFVal(PDFMeasure::ProjectedSolidAngle, 0_f);
        }

        return Sampler::CosineSampleHemispherePDFProjSA(localWo);
    };

    LM_IMPL_F(EvaluateDirection) = [this](const SurfaceGeometry& geom, int types, const Vec3& wi, const Vec3& wo, TransportDirection transDir, bool evalDelta) -> SPD
    {
        const auto localWi = geom.ToLocal * wi;
        const auto localWo = geom.ToLocal * wo;
        if (Math::LocalCos(localWi) <= 0_f || Math::LocalCos(localWo) <= 0_f)
        {
            return SPD();
        }

        const auto R = texR_ ? SPD::FromRGB(texR_->Evaluate(geom.uv)) : R_;
        return R * Math::InvPi() * BSDFUtils::ShadingNormalCorrection(geom, wi, wo, transDir);
    };

    LM_IMPL_F(IsDeltaDirection) = [this](int type) -> bool
    {
        return false;
    };

public:

    SPD R_;
    const Texture* texR_ = nullptr;

};

LM_COMPONENT_REGISTER_IMPL(BSDF_Diffuse, "bsdf::diffuse");

LM_NAMESPACE_END
