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
#include <lightmetrica/detail/subpathsampler.h>
#include <fstream>
#include <boost/format.hpp>
#include <boost/optional.hpp>
#include <boost/filesystem.hpp> 

#if LM_DEBUG_MODE
#define INVERSEMAP_OMIT_NORMALIZATION 1
#else
#define INVERSEMAP_OMIT_NORMALIZATION 1
#endif

LM_NAMESPACE_BEGIN

struct Subpath
{
    std::vector<SubpathSampler::PathVertex> vertices;

    auto SampleSubpathFromEndpoint(const Scene* scene, Random* rng, TransportDirection transDir, int maxNumVertices) -> int
    {
        const auto  n = (int)(vertices.size());
        const auto* pv = n > 0 ? &vertices[n - 1] : nullptr;
        const auto* ppv = n > 1 ? &vertices[n - 2] : nullptr;
        SubpathSampler::TraceSubpathFromEndpoint(scene, rng, pv, ppv, n, n + maxNumVertices, transDir, [&](int numVertices, const Vec2& /*rasterPos*/, const SubpathSampler::SubpathSampler::PathVertex& pv, const SubpathSampler::SubpathSampler::PathVertex& v, SPD& throughput) -> bool
        {
            vertices.emplace_back(v);
            return true;
        });
        return (int)(vertices.size()) - n;
    }

    auto BeginWith(const std::string& types) const -> bool
    {
        const auto PathType = [](const SubpathSampler::PathVertex& v) -> char
        {
            switch (v.type)
            {
                case SurfaceInteractionType::D: return 'D';
                case SurfaceInteractionType::G: return 'G';
                case SurfaceInteractionType::S: return 'S';
                case SurfaceInteractionType::L: return 'L';
                case SurfaceInteractionType::E: return 'E';
                default: return 'X';
            }
        };
        if (types.size() > vertices.size())
        {
            return false;
        }
        for (size_t i = 0; i < vertices.size(); i++)
        {
            if (types[i] != PathType(vertices[i]))
            {
                return false;
            }
        }
        return true;
    }

};

struct Path
{
    std::vector<SubpathSampler::PathVertex> vertices;

    auto IsPathType(const std::string& types) const -> bool
    {
        const auto PathType = [](const SubpathSampler::PathVertex& v) -> char
        {
            switch (v.type)
            {
                case SurfaceInteractionType::D: return 'D';
                case SurfaceInteractionType::G: return 'G';
                case SurfaceInteractionType::S: return 'S';
                case SurfaceInteractionType::L: return 'L';
                case SurfaceInteractionType::E: return 'E';
                default: return 'X';
            }
        };
        if (types.size() > vertices.size())
        {
            return false;
        }
        for (size_t i = 0; i < vertices.size(); i++)
        {
            if (types[i] != PathType(vertices[i]))
            {
                return false;
            }
        }
        return true;
    }

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

    auto EvaluateUnweightContribution(const Scene* scene, int s) const -> SPD
    {
        const int n = (int)(vertices.size());
        const int t = n - s;

        // --------------------------------------------------------------------------------

        #pragma region Compute alphaL

        #if 0
        SPD alphaL;
        if (s == 0)
        {
            alphaL = SPD(1_f);
        }
        else
        {
            {
                const auto* v = &vertices[0];
                const auto* vNext = &vertices[1];
                alphaL =
                    v->primitive->EvaluatePosition(v->geom, false) /
                    v->primitive->EvaluatePositionGivenDirectionPDF(v->geom, Math::Normalize(vNext->geom.p - v->geom.p), false) / scene->EvaluateEmitterPDF(v->primitive).v;
            }
            for (int i = 0; i < s - 1; i++)
            {
                const auto* v     = &vertices[i];
                const auto* vPrev = i >= 1 ? &vertices[i - 1] : nullptr;
                const auto* vNext = &vertices[i + 1];
                const auto wi = vPrev ? Math::Normalize(vPrev->geom.p - v->geom.p) : Vec3();
                const auto wo = Math::Normalize(vNext->geom.p - v->geom.p);
                const auto fs = v->primitive->EvaluateDirection(v->geom, v->type, wi, wo, TransportDirection::LE, false);
                if (fs.Black()) return SPD();
                alphaL *= fs / v->primitive->EvaluateDirectionPDF(v->geom, v->type, wi, wo, false);
            }
        }
        if (alphaL.Black())
        {
            return SPD();
        }
        #else
        const auto alphaL = EvaluateAlpha(scene, s, TransportDirection::LE);
        if (alphaL.Black())
        {
            return SPD();
        }
        #endif

        #pragma endregion

        // --------------------------------------------------------------------------------

        #pragma region Compute alphaE

        #if 0
        SPD alphaE;
        if (t == 0)
        {
            alphaE = SPD(1_f);
        }
        else
        {
            {
                const auto* v = &vertices[n - 1];
                const auto* vPrev = &vertices[n - 2];
                alphaE =
                    v->primitive->EvaluatePosition(v->geom, false) /
                    v->primitive->EvaluatePositionGivenDirectionPDF(v->geom, Math::Normalize(vPrev->geom.p - v->geom.p), false) / scene->EvaluateEmitterPDF(v->primitive).v;
            }
            for (int i = n - 1; i > s; i--)
            {
                const auto* v = &vertices[i];
                const auto* vPrev = &vertices[i - 1];
                const auto* vNext = i < n - 1 ? &vertices[i + 1] : nullptr;
                const auto wi = vNext ? Math::Normalize(vNext->geom.p - v->geom.p) : Vec3();
                const auto wo = Math::Normalize(vPrev->geom.p - v->geom.p);
                const auto fs = v->primitive->EvaluateDirection(v->geom, v->type, wi, wo, TransportDirection::EL, false);
                if (fs.Black()) return SPD();
                alphaE *= fs / v->primitive->EvaluateDirectionPDF(v->geom, v->type, wi, wo, false);
            }
        }
        if (alphaE.Black())
        {
            return SPD();
        }
        #else
        const auto alphaE = EvaluateAlpha(scene, t, TransportDirection::EL);
        if (alphaE.Black())
        {
            return SPD();
        }
        #endif

        #pragma endregion

        // --------------------------------------------------------------------------------

        #pragma region Compute Cst

        const auto cst = EvaluateCst(s);
        if (cst.Black())
        {
            return SPD();
        }

        #pragma endregion

        // --------------------------------------------------------------------------------

        return alphaL * cst * alphaE;
    }

    auto EvaluateAlpha(const Scene* scene, int l, TransportDirection transDir) const -> SPD
    {
        const int n = (int)(vertices.size());
        const auto index = [&](int i)
        {
            return transDir == TransportDirection::LE ? i : n - 1 - i;
        };

        SPD alpha;
        if (l == 0)
        {
            alpha = SPD(1_f);
        }
        else
        {
            {
                const auto* v = &vertices[index(0)];
                const auto* vNext = &vertices[index(1)];
                alpha =
                    v->primitive->EvaluatePosition(v->geom, false) /
                    v->primitive->EvaluatePositionGivenDirectionPDF(v->geom, Math::Normalize(vNext->geom.p - v->geom.p), false) / scene->EvaluateEmitterPDF(v->primitive).v;
            }
            for (int i = 0; i < l - 1; i++)
            {
                const auto* v = &vertices[index(i)];
                const auto* vPrev = index(i - 1) >= 0 && index(i - 1) < n ? &vertices[index(i - 1)] : nullptr;
                const auto* vNext = index(i + 1) >= 0 && index(i + 1) < n ? &vertices[index(i + 1)] : nullptr;
                assert(vPrev != nullptr || vNext != nullptr);
                const auto wi = vPrev ? Math::Normalize(vPrev->geom.p - v->geom.p) : Vec3();
                const auto wo = vNext ? Math::Normalize(vNext->geom.p - v->geom.p) : Vec3();
                const auto fs = v->primitive->EvaluateDirection(v->geom, v->type, wi, wo, transDir, false);
                if (fs.Black()) return SPD();
                alpha *= fs / v->primitive->EvaluateDirectionPDF(v->geom, v->type, wi, wo, false);
            }
        }
        if (alpha.Black())
        {
            return SPD();
        }

        return alpha;
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

    auto EvaluateCst(int s) const -> SPD
    {
        const int n = (int)(vertices.size());
        const int t = n - s;
        assert(n >= 2);

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

        return cst;
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
        SubpathSampler::PathVertex pv, ppv;
        Path path;
        int samplerIndex = 0;
        const int maxNumVertices = (int)primarySample.size() / 2 + 1;
        for (int step = 0; step < maxNumVertices; step++)
        {
            if (step == 0)
            {
                #pragma region Sample initial vertex

                SubpathSampler::PathVertex v;

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

                #pragma endregion
            }
            else
            {
                #pragma region Sample intermediate vertex

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
                    // Random number for the component selection is fixed to u1, current implementation only supports
                    // the component selection of flesnel material. This case does not need the direction samples.
                    wi = Math::Normalize(ppv.geom.p - pv.geom.p);
                    const auto u1 = primarySample[samplerIndex++];
                    const auto u2 = primarySample[samplerIndex++];
                    pv.primitive->SampleDirection(Vec2(u1, u2), u1, pv.type, pv.geom, wi, wo);
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
                //assert(std::strcmp(isect.primitive->bsdf->implName, "BSDF_Diffuse") == 0 ||
                //       std::strcmp(isect.primitive->bsdf->implName, "BSDF_CookTorrance") == 0);

                // Add a vertex
                SubpathSampler::PathVertex v;
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

                #pragma endregion
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

    static auto MapPath2PS(const Path& inputPath)->std::vector<Float>
    {
        return MapPath2PS(inputPath, nullptr);
    }

    static auto MapPath2PS(const Path& inputPath, Random* rng) -> std::vector<Float>
    {
        #pragma region Helper function

        #if 0
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
        #endif

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
                assert(v->type != SurfaceInteractionType::L);
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
                        //const auto inv = SampleBechmannDist_Inverse(H, roughness);
                        const auto inv = SampleGGX_Inverse(roughness, H);
                        ps.push_back(inv.x);
                        ps.push_back(inv.y);
                    }
                    else if (v->type == SurfaceInteractionType::S)
                    {
                        // TODO: Replace it. String comparision is too slow.
                        if (std::strcmp(v->primitive->bsdf->implName, "BSDF_ReflectAll") == 0)
                        {
                            // Deterministic computation of reflected directions breaks one-to-one mapping,
                            // here we decide the next direction with filling with new random numbers.
                            // If the mutation does not changes path types, we can set arbitrary values.
                            ps.push_back(rng->Next());
                            ps.push_back(rng->Next());
                        }
                        else if (std::strcmp(v->primitive->bsdf->implName, "BSDF_RefractAll") == 0)
                        {
                            ps.push_back(rng->Next());
                            ps.push_back(rng->Next());
                        }
                        else if (std::strcmp(v->primitive->bsdf->implName, "BSDF_Flesnel") == 0)
                        {
                            const auto Fr = v->primitive->bsdf->FlesnelTerm(v->geom, wi);
                            const auto localWi = v->geom.ToLocal * wi;
                            const auto localWo = v->geom.ToLocal * wo;
                            if (Math::LocalCos(localWi) * Math::LocalCos(localWo) >= 0_f)
                            {
                                // Reflection
                                // Set u <= Fr
                                ps.push_back(rng->Next() * (Fr - Math::Eps()));
                            }
                            else
                            {
                                // Refraction
                                // Set u > Fr
                                ps.push_back(Math::Eps() + Fr + rng->Next() * (1_f - Fr - Math::Eps()));
                            }
                            // Arbitrary number
                            ps.push_back(rng->Next());
                        }
                    }
                }
            }
        }

        return ps;
    }

    static auto UniformConcentricDiskSample_Inverse(const Vec2& s) -> Vec2
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
    }

    static auto SampleGGX_Inverse(Float roughness_, const Vec3& H) -> Vec2
    {
        const auto u0 = [&]() {
            const auto tanTheta2 = Math::LocalTan2(H);
            if (tanTheta2 == Math::Inf()) return 1_f;
            return tanTheta2 / (tanTheta2 + roughness_ * roughness_);
        }();

        const auto phiH = [&]() {
            const auto t = std::atan2(H.y, H.x);
            return t;
        }();
        const auto u1 = (phiH * Math::InvPi() + 1_f) * 0.5_f;

        return Vec2(u0, u1);
    }

    ///! Number of samples required for the underlying path sampler.
    static auto NumSamples(int numVertices) -> int
    {
        return (numVertices - 1) * 2;
    }

    ///! Scalar contirbution function
    static auto ScalarContrb(const SPD& w) -> Float
    {
        // OK
        //return w.v.z;
        //return w.v.x;
        // Wrong
        //return Math::Max(w.v.x, Math::Max(w.v.y, w.v.z));
        return w.Luminance();
        //return w.v.x + w.v.y + w.v.z;
        //return Math::Max(w.v.x, w.v.y);
    }

};

LM_NAMESPACE_END