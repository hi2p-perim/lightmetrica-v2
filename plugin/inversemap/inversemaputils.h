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

#pragma once

#include <lightmetrica/lightmetrica.h>
#include <lightmetrica/detail/parallel.h>
#include <fstream>
#include <boost/format.hpp>
#include <boost/optional.hpp>
#include <boost/filesystem.hpp>

#define INVERSEMAP_OMIT_NORMALIZATION 0

LM_NAMESPACE_BEGIN

struct PathVertex
{
    int type;
    SurfaceGeometry geom;
    const Primitive* primitive = nullptr;
};

struct Subpath
{
    std::vector<PathVertex> vertices;
    auto SampleSubpathFromEndpoint(const Scene* scene, Random* rng, TransportDirection transDir, int maxNumVertices) -> int
    {
        Vec3 initWo;
        SPD throughput;
        int step;
        for (step = 0; step < maxNumVertices; step++)
        {
            const int n = (int)(vertices.size());
            if (n == 0)
            {
                #pragma region Sample initial vertex

                PathVertex v;

                // Sample an emitter
                v.type = transDir == TransportDirection::LE ? SurfaceInteractionType::L : SurfaceInteractionType::E;
                v.primitive = scene->SampleEmitter(v.type, rng->Next());

                // Sample a position on the emitter and initial ray direction
                v.primitive->SamplePositionAndDirection(rng->Next2D(), rng->Next2D(), v.geom, initWo);

                // Initial throughput
                throughput =
                    v.primitive->EvaluatePosition(v.geom, false) /
                    v.primitive->EvaluatePositionGivenDirectionPDF(v.geom, initWo, false) /
                    scene->EvaluateEmitterPDF(v.primitive);

                // Process vertex
                vertices.emplace_back(v);

                #pragma endregion
            }
            else
            {
                #pragma region Sample a vertex with PDF with BSDF

                // Previous & two before vertex
                const auto* pv = &vertices[n - 1];
                const auto* ppv = n > 1 ? &vertices[n - 2] : nullptr;

                // Sample a next direction
                Vec3 wi;
                Vec3 wo;
                if (n == 1)
                {
                    assert(step == 0 || step == 1);
                    if (step == 1)
                    {
                        // Initial direction is sampled from joint distribution
                        wi = Vec3();
                        wo = initWo;
                    }
                    else
                    {
                        // Sample if the surface support sampling from $p_{\sigma^\perp}(\omega_o | \mathbf{x})$
                        assert(pv->primitive->emitter);
                        if (!pv->primitive->emitter->SampleDirection.Implemented()) { break; }
                        pv->primitive->SampleDirection(rng->Next2D(), rng->Next(), pv->type, pv->geom, Vec3(), wo);
                    }
                }
                else
                {
                    assert(ppv);
                    wi = Math::Normalize(ppv->geom.p - pv->geom.p);
                    pv->primitive->SampleDirection(rng->Next2D(), rng->Next(), pv->type, pv->geom, wi, wo);
                }

                // Evaluate direction
                const auto fs = pv->primitive->EvaluateDirection(pv->geom, pv->type, wi, wo, transDir, false);
                if (fs.Black())
                {
                    break;
                }
                const auto pdfD = pv->primitive->EvaluateDirectionPDF(pv->geom, pv->type, wi, wo, false);
                assert(pdfD > 0_f);

                // Update throughput
                throughput *= fs / pdfD;

                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Intersection query
                Ray ray = { pv->geom.p, wo };
                Intersection isect;
                if (!scene->Intersect(ray, isect))
                {
                    break;
                }
                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Process path vertex
                PathVertex v;
                v.geom = isect.geom;
                v.primitive = isect.primitive;
                v.type = isect.primitive->Type() & ~SurfaceInteractionType::Emitter;
                vertices.push_back(v);
                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Path termiantion
                if (isect.geom.infinite)
                {
                    break;
                }
                #pragma endregion
            }
        }
        return step;
    }
};

struct Path
{
    std::vector<PathVertex> vertices;

    auto ConnectSubpaths(const Scene* scene, const Subpath& subpathL, const Subpath& subpathE, int s, int t) -> bool
    {
        assert(s >= 0);
        assert(t >= 0);
        vertices.clear();
        if (s == 0 && t > 0)
        {
            vertices.insert(vertices.end(), subpathE.vertices.rend() - t, subpathE.vertices.rend());
            if ((vertices.front().primitive->Type() & SurfaceInteractionType::L) == 0) { return false; }
            vertices.front().type = SurfaceInteractionType::L;
        }
        else if (s > 0 && t == 0)
        {
            vertices.insert(vertices.end(), subpathL.vertices.begin(), subpathL.vertices.begin() + s);
            if ((vertices.back().primitive->Type() & SurfaceInteractionType::E) == 0) { return false; }
            vertices.back().type = SurfaceInteractionType::E;
        }
        else
        {
            const auto& vL = subpathL.vertices[s - 1];
            const auto& vE = subpathE.vertices[t - 1];
            if (vL.geom.infinite || vE.geom.infinite) { return false; }
            if (!scene->Visible(vL.geom.p, vE.geom.p)) { return false; }
            vertices.insert(vertices.end(), subpathL.vertices.begin(), subpathL.vertices.begin() + s);
            vertices.insert(vertices.end(), subpathE.vertices.rend() - t, subpathE.vertices.rend());
        }
        return true;
    }

    auto EvaluateF(int s) const -> SPD
    {
        const int n = (int)(vertices.size());
        const int t = n - s;
        assert(n >= 2);

        // --------------------------------------------------------------------------------

        SPD fL;
        if (s == 0) { fL = SPD(1_f); }
        else
        {
            {
                const auto* vL = &vertices[0];
                fL = vL->primitive->EvaluatePosition(vL->geom, false);
            }
            for (int i = 0; i < s - 1; i++)
            {
                const auto* v = &vertices[i];
                const auto* vPrev = i >= 1 ? &vertices[i - 1] : nullptr;
                const auto* vNext = &vertices[i + 1];
                const auto wi = vPrev ? Math::Normalize(vPrev->geom.p - v->geom.p) : Vec3();
                const auto wo = Math::Normalize(vNext->geom.p - v->geom.p);
                fL *= v->primitive->EvaluateDirection(v->geom, v->type, wi, wo, TransportDirection::LE, false);
                fL *= RenderUtils::GeometryTerm(v->geom, vNext->geom);
            }
        }
        if (fL.Black()) { return SPD(); }

        // --------------------------------------------------------------------------------

        SPD fE;
        if (t == 0) { fE = SPD(1_f); }
        else
        {
            {
                const auto* vE = &vertices[n - 1];
                fE = vE->primitive->EvaluatePosition(vE->geom, false);
            }
            for (int i = n - 1; i > s; i--)
            {
                const auto* v = &vertices[i];
                const auto* vPrev = &vertices[i - 1];
                const auto* vNext = i < n - 1 ? &vertices[i + 1] : nullptr;
                const auto wi = vNext ? Math::Normalize(vNext->geom.p - v->geom.p) : Vec3();
                const auto wo = Math::Normalize(vPrev->geom.p - v->geom.p);
                fE *= v->primitive->EvaluateDirection(v->geom, v->type, wi, wo, TransportDirection::EL, false);
                fE *= RenderUtils::GeometryTerm(v->geom, vPrev->geom);
            }
        }
        if (fE.Black()) { return SPD(); }

        // --------------------------------------------------------------------------------

        SPD cst;
        if (s == 0 && t > 0)
        {
            const auto& v = vertices[0];
            const auto& vNext = vertices[1];
            cst = v.primitive->EvaluatePosition(v.geom, true) * v.primitive->EvaluateDirection(v.geom, v.type, Vec3(), Math::Normalize(vNext.geom.p - v.geom.p), TransportDirection::EL, false);
        }
        else if (s > 0 && t == 0)
        {
            const auto& v = vertices[n - 1];
            const auto& vPrev = vertices[n - 2];
            cst = v.primitive->EvaluatePosition(v.geom, true) * v.primitive->EvaluateDirection(v.geom, v.type, Vec3(), Math::Normalize(vPrev.geom.p - v.geom.p), TransportDirection::LE, false);
        }
        else if (s > 0 && t > 0)
        {
            const auto* vL = &vertices[s - 1];
            const auto* vE = &vertices[s];
            const auto* vLPrev = s - 2 >= 0 ? &vertices[s - 2] : nullptr;
            const auto* vENext = s + 1 < n ? &vertices[s + 1] : nullptr;
            const auto fsL = vL->primitive->EvaluateDirection(vL->geom, vL->type, vLPrev ? Math::Normalize(vLPrev->geom.p - vL->geom.p) : Vec3(), Math::Normalize(vE->geom.p - vL->geom.p), TransportDirection::LE, true);
            const auto fsE = vE->primitive->EvaluateDirection(vE->geom, vE->type, vENext ? Math::Normalize(vENext->geom.p - vE->geom.p) : Vec3(), Math::Normalize(vL->geom.p - vE->geom.p), TransportDirection::EL, true);
            const Float G = RenderUtils::GeometryTerm(vL->geom, vE->geom);
            cst = fsL * G * fsE;
        }

        // --------------------------------------------------------------------------------

        return fL * cst * fE;
    }

    auto EvaluatePathPDF(const Scene* scene, int s) const -> PDFVal
    {
        const int n = (int)(vertices.size());
        const int t = n - s;
        assert(n >= 2);

        // Check if the path is samplable by vertex connection
        if (s == 0 && t > 0)
        {
            const auto& v = vertices[0];
            if (v.primitive->IsDeltaPosition(v.type)) { return PDFVal(PDFMeasure::ProdArea, 0_f); }
        }
        else if (s > 0 && t == 0)
        {
            const auto& v = vertices[n - 1];
            if (v.primitive->IsDeltaPosition(v.type)) { return PDFVal(PDFMeasure::ProdArea, 0_f); }
        }
        else if (s > 0 && t > 0)
        {
            const auto& vL = vertices[s - 1];
            const auto& vE = vertices[s];
            if (vL.primitive->IsDeltaDirection(vL.type) || vE.primitive->IsDeltaDirection(vE.type)) { return PDFVal(PDFMeasure::ProdArea, 0_f); }
        }

        // Otherwise the path can be generated with the given strategy (s,t,merge) so p_{s,t,merge} can be safely evaluated.
        PDFVal pdf(PDFMeasure::ProdArea, 1_f);
        if (s > 0)
        {
            pdf *= vertices[0].primitive->EvaluatePositionGivenDirectionPDF(vertices[0].geom, Math::Normalize(vertices[1].geom.p - vertices[0].geom.p), false) * scene->EvaluateEmitterPDF(vertices[0].primitive).v;
            for (int i = 0; i < s - 1; i++)
            {
                const auto* vi = &vertices[i];
                const auto* vip = i - 1 >= 0 ? &vertices[i - 1] : nullptr;
                const auto* vin = &vertices[i + 1];
                pdf *= vi->primitive->EvaluateDirectionPDF(vi->geom, vi->type, vip ? Math::Normalize(vip->geom.p - vi->geom.p) : Vec3(), Math::Normalize(vin->geom.p - vi->geom.p), false).ConvertToArea(vi->geom, vin->geom);
            }
        }
        if (t > 0)
        {
            pdf *= vertices[n - 1].primitive->EvaluatePositionGivenDirectionPDF(vertices[n - 1].geom, Math::Normalize(vertices[n - 2].geom.p - vertices[n - 1].geom.p), false) * scene->EvaluateEmitterPDF(vertices[n - 1].primitive).v;
            for (int i = n - 1; i >= s + 1; i--)
            {
                const auto* vi = &vertices[i];
                const auto* vip = &vertices[i - 1];
                const auto* vin = i + 1 < n ? &vertices[i + 1] : nullptr;
                pdf *= vi->primitive->EvaluateDirectionPDF(vi->geom, vi->type, vin ? Math::Normalize(vin->geom.p - vi->geom.p) : Vec3(), Math::Normalize(vip->geom.p - vi->geom.p), false).ConvertToArea(vi->geom, vip->geom);
            }
        }

        return pdf;
    }

    auto EvaluateMISWeight(const Scene* scene, int s_) const -> Float
    {
        const int n = static_cast<int>(vertices.size());
        const auto ps = EvaluatePathPDF(scene, s_);
        assert(ps > 0_f);

        Float invw = 0_f;
        for (int s = 0; s <= n; s++)
        {
            const auto pi = EvaluatePathPDF(scene, s);
            if (pi > 0_f)
            {
                const auto r = pi.v / ps.v;
                invw += r*r;
            }
        }

        return 1_f / invw;
    }

    auto RasterPosition() const -> Vec2
    {
        const auto& v = vertices[vertices.size() - 1];
        const auto& vPrev = vertices[vertices.size() - 2];
        Vec2 rasterPos;
        v.primitive->sensor->RasterPosition(Math::Normalize(vPrev.geom.p - v.geom.p), v.geom, rasterPos);
        return rasterPos;
    }

};

// --------------------------------------------------------------------------------

class TwoTailedGeometricDist
{
private:

    Float base_;
    Float invLogBase_;
    Float baseNormalization_;

    int center_, start_, end_;
    Float offset_;
    Float normalization_;

public:

    TwoTailedGeometricDist(Float base)
        : base_(base)
    {
        baseNormalization_ = 1_f / (base + 1_f);
        invLogBase_ = 1_f / std::log(base);
    }

public:

    auto Configure(int center, int start, int end) -> void
    {
        center_ = center;
        start_ = start - center;
        end_ = end - center;
        offset_ = R(this->start_ - 1);
        normalization_ = R(this->end_) - offset_;
    }

    auto EvaluatePDF(int i) const -> Float
    {
        i -= center_;
        if (i < start_ || i > end_) { return 0_f; }
        return r(i) / normalization_;
    }

    auto EvaluateCDF(int i) const -> Float
    {
        i -= center_;
        if (i < start_) { return 0_f; }
        else if (i > end_) { i = end_; }
        return (R(i) - offset_) / normalization_;
    }

    auto Sample(Float u) const -> int
    {
        // For rare case u=1 generates divide by zero exception
        u = Math::Clamp(u, 0_f, 1_f - Math::Eps());
        return Math::Max(start_, Rinv(u * normalization_ + offset_)) + center_;
    }

private:

    auto r(int i) const -> Float
    {
        //RF_DISABLE_FP_EXCEPTION();
        const Float t = (base_ - 1_f) * baseNormalization_ * std::pow(base_, -std::abs((Float)(i)));
        //RF_ENABLE_FP_EXCEPTION();
        return t;
    }

    auto R(int i) const -> Float
    {
        //RF_DISABLE_FP_EXCEPTION();
        const Float t = i <= 0 ? std::pow(base_, (Float)(i + 1)) * baseNormalization_ : 1_f - std::pow(base_, -(Float)(i)) * baseNormalization_;
        //RF_ENABLE_FP_EXCEPTION();
        return t;
    }

    auto Rinv(Float x) const -> int
    {
        Float result;
        if (x < base_ * baseNormalization_)
        {
            result = std::log((1_f + base_) * x) * invLogBase_ - 1_f;
        }
        else
        {
            result = -std::log((1_f + base_) * (1_f - x)) * invLogBase_;
        }
        return static_cast<int>(std::ceil(result));
    }

};

// --------------------------------------------------------------------------------

///! Utility class for inversemap project
class InversemapUtils
{
public:

    LM_DISABLE_CONSTRUCT(InversemapUtils);

public:

    ///! Returns boost::none for invalid paths for early rejection.
    static auto MapPS2Path(const Scene* scene, const std::vector<Float>& primarySample) -> boost::optional<Path>
    {
        Vec3 initWo;
        PathVertex pv, ppv;
        Path path;
        int samplerIndex = 0;
        const int maxNumVertices = (int)primarySample.size() / 2 + 1;
        for (int step = 0; step < maxNumVertices; step++)
        {
            if (step == 0)
            {
                //region Sample initial vertex

                PathVertex v;

                // Emitter is fixed (initial one is used)
                v.type = SurfaceInteractionType::E;
                v.primitive = scene->GetSensor();

                // Assume the sensor is pinhole camera
                assert(std::strcmp(v.primitive->emitter->implName, "Sensor_Pinhole") == 0);

                // Sample a position on the emitter and initial ray direction
                //const auto u = Vec2(primarySample[samplerIndex++], primarySample[samplerIndex++]);
                const auto u1 = primarySample[samplerIndex++];
                const auto u2 = primarySample[samplerIndex++];
                v.primitive->SamplePositionAndDirection(Vec2(u1, u2), Vec2(), v.geom, initWo);

                // Create a vertex
                path.vertices.push_back(v);

                // Update information
                pv = v;

                //endregion
            }
            else
            {
                //region Sample intermediate vertex

                // Sample a next direction
                Vec3 wi;
                Vec3 wo;
                if (step == 1)
                {
                    wi = Vec3();
                    wo = initWo;
                }
                else
                {
                    // Random number for the component selection is fixed to zero, meaning no support for this kind of materials
                    wi = Math::Normalize(ppv.geom.p - pv.geom.p);
                    const auto u1 = primarySample[samplerIndex++];
                    const auto u2 = primarySample[samplerIndex++];
                    pv.primitive->SampleDirection(Vec2(u1, u2), 0_f, pv.type, pv.geom, wi, wo);
                }

                // Evaluate direction
                const auto fs = pv.primitive->EvaluateDirection(pv.geom, pv.type, wi, wo, TransportDirection::EL, false);
                if (fs.Black())
                {
                    break;
                }

                #if 0
                if (std::strcmp(pv.primitive->id, "n5") == 0)
                {
                    static long long count = 0;
                    if (count == 0)
                    {
                        boost::filesystem::remove("dirs.out");
                    }
                    if (count < 100)
                    {
                        count++;
                        std::ofstream out("dirs.out", std::ios::out | std::ios::app);
                        out << boost::str(boost::format("%.10f %.10f %.10f ") % pv.geom.p.x % pv.geom.p.y % pv.geom.p.z);
                        const auto p = pv.geom.p + wo;
                        out << boost::str(boost::format("%.10f %.10f %.10f ") % p.x % p.y % p.z);
                        out << std::endl;
                    }

                }
                #endif

                // Intersection query
                Ray ray = { pv.geom.p, wo };
                Intersection isect;
                if (!scene->Intersect(ray, isect))
                {
                    break;
                }

                // Assume all surface is diffuse or glossy
                assert(std::strcmp(isect.primitive->bsdf->implName, "BSDF_Diffuse") == 0 ||
                       std::strcmp(isect.primitive->bsdf->implName, "BSDF_CookTorrance") == 0);

                // Add a vertex
                PathVertex v;
                v.geom = isect.geom;
                v.primitive = isect.primitive;
                v.type = isect.primitive->Type() & ~SurfaceInteractionType::Emitter;
                path.vertices.push_back(v);

                // Path termination
                if (isect.geom.infinite)
                {
                    break;
                }

                // Update information
                ppv = pv;
                pv = v;

                //endregion
            }
        }

        if ((path.vertices.back().primitive->Type() & SurfaceInteractionType::L) == 0)
        {
            return boost::none;
        }
        path.vertices.back().type = SurfaceInteractionType::L;

        std::reverse(path.vertices.begin(), path.vertices.end());

        return path;
    }

    static auto MapPath2PS(const Path& inputPath) -> std::vector<Float>
    {
        #pragma region Helper function
        const auto UniformConcentricDiskSample_Inverse = [](const Vec2& s) -> Vec2
        {
            const auto r = std::sqrt(s.x*s.x + s.y*s.y);
            auto theta = std::atan2(s.y, s.x);
            Vec2 u;
            if (s.x > -s.y)
            {
                if (s.x > s.y)
                {
                    u.x = r;
                    u.y = 4_f * theta * r * Math::InvPi();
                }
                else
                {
                    u.y = r;
                    u.x = (2_f - 4_f * theta * Math::InvPi()) * r;
                }
            }
            else
            {
                theta = theta < 0_f ? theta + 2_f * Math::Pi() : theta;
                if (s.x < s.y)
                {
                    u.x = -r;
                    u.y = (4_f - 4_f * theta * Math::InvPi()) * r;
                }
                else
                {
                    u.y = -r;
                    u.x = (-6_f + 4_f * theta * Math::InvPi()) * r;
                }
            }
            return (u + Vec2(1_f)) * 0.5_f;
        };

        const auto SampleBechmannDist_Inverse = [](const Vec3& H, Float roughness) -> Vec2
        {
            const auto u0 = [&]() -> Float
            {
                const auto cosThetaH = Math::LocalCos(H);
                if (cosThetaH * cosThetaH < Math::Eps()) return 1_f;
                const auto tanThetaHSqr = 1_f / (cosThetaH * cosThetaH) - 1_f;
                const auto exp = std::exp(-tanThetaHSqr / (roughness * roughness));
                return 1_f - exp;
            }();

            const auto sinThetaH = Math::LocalSin(H);
            const auto cosPhiH = H.x / sinThetaH;
            const auto sinPhiH = H.y / sinThetaH;
            const auto phiH = [&]() {
                const auto t = std::atan2(sinPhiH, cosPhiH);
                return t < 0_f ? t + 2_f * Math::Pi() : t;
            }();
            const auto u1 = phiH * 0.5_f * Math::InvPi();

            return Vec2(u0, u1);
        };
        #pragma endregion

        // --------------------------------------------------------------------------------

        std::vector<Float> ps;
        auto path = inputPath;
        std::reverse(path.vertices.begin(), path.vertices.end());

        for (size_t i = 0; i < path.vertices.size(); i++)
        {
            const auto* v = &path.vertices[i];
            const auto* vn = i + 1 < path.vertices.size() ? &path.vertices[i + 1] : nullptr;
            const auto* vp = i > 0 ? &path.vertices[i - 1] : nullptr;

            if (i == 0)
            {
                // No sample is needed for the pinhole camera
                assert(std::strcmp(v->primitive->emitter->implName, "Sensor_Pinhole") == 0);
            }

            if (vn)
            {
                const auto wo = Math::Normalize(vn->geom.p - v->geom.p);
                assert(v->type == SurfaceInteractionType::E || v->type == SurfaceInteractionType::D || v->type == SurfaceInteractionType::G);
                if (v->type == SurfaceInteractionType::E)
                {
                    Vec2 inv;
                    v->primitive->sensor->RasterPosition(wo, v->geom, inv);
                    //inv = (inv + Vec2(1_f)) * 0.5_f;
                    ps.push_back(inv.x);
                    ps.push_back(inv.y);
                }
                else
                {
                    assert(vp != nullptr);
                    const auto wi = Math::Normalize(vp->geom.p - v->geom.p);
                    if (v->type == SurfaceInteractionType::D)
                    {
                        const auto localWo = v->geom.ToLocal * wo;
                        const auto inv = UniformConcentricDiskSample_Inverse(Vec2(localWo.x, localWo.y));
                        ps.push_back(inv.x);
                        ps.push_back(inv.y);
                    }
                    else if (v->type == SurfaceInteractionType::G)
                    {
                        const auto localWi = v->geom.ToLocal * wi;
                        const auto localWo = v->geom.ToLocal * wo;
                        const auto H = Math::Normalize(localWi + localWo);
                        const auto roughness = v->primitive->bsdf->Glossiness();
                        const auto inv = SampleBechmannDist_Inverse(H, roughness);
                        ps.push_back(inv.x);
                        ps.push_back(inv.y);
                    }
                }
            }
        }

        return ps;
    }

    ///! Number of samples required for the underlying path sampler.
    static auto NumSamples(int numVertices) -> int
    {
        return (numVertices - 1) * 2;
    }

};

LM_NAMESPACE_END