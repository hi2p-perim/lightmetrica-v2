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
#include <lightmetrica/renderer.h>
#include <lightmetrica/property.h>
#include <lightmetrica/random.h>
#include <lightmetrica/scene.h>
#include <lightmetrica/film.h>
#include <lightmetrica/bsdf.h>
#include <lightmetrica/ray.h>
#include <lightmetrica/intersection.h>
#include <lightmetrica/emitter.h>
#include <lightmetrica/sensor.h>
#include <lightmetrica/surfacegeometry.h>
#include <lightmetrica/primitive.h>
#include <lightmetrica/scheduler.h>
#include <lightmetrica/renderutils.h>
#include <tbb/tbb.h>

#define LM_BDPT_DEBUG 0

LM_NAMESPACE_BEGIN

// --------------------------------------------------------------------------------

struct Path;

struct MISWeight : public Component
{
    LM_INTERFACE_CLASS(MISWeight, Component, 1);
    LM_INTERFACE_F(0, Evaluate, Float(const Path& path, const Scene* scene, int s, bool direct));
};

// --------------------------------------------------------------------------------

#pragma region Path structures

struct PathVertex
{
    int type;
    SurfaceGeometry geom;
    const Primitive* primitive = nullptr;
};

struct SubpathVertex
{
    boost::optional<PathVertex> sv;
    boost::optional<PathVertex> direct;
};

struct Subpath
{

    std::vector<SubpathVertex> vertices;

public:

    auto Sample(const Scene* scene, Random* rng, TransportDirection transDir, int maxPathVertices) -> void
    {
        vertices.clear();

        // --------------------------------------------------------------------------------

        Vec3 initWo;
        for (int step = 0; maxPathVertices == -1 || step < maxPathVertices; step++)
        {
            if (step == 0)
            {
                #pragma region Sample initial vertex

                PathVertex sv;

                // Sample an emitter
                sv.type = transDir == TransportDirection::LE ? SurfaceInteractionType::L : SurfaceInteractionType::E;
                sv.primitive = scene->SampleEmitter(sv.type, rng->Next());

                // Sample a position on the emitter and initial ray direction
                sv.primitive->SamplePositionAndDirection(rng->Next2D(), rng->Next2D(), sv.geom, initWo);

                // Add a vertex
                SubpathVertex v;
                v.sv = sv;
                vertices.push_back(v);

                #pragma endregion
            }
            else
            {
                #pragma region Sample a vertex with PDF with BSDF

                const auto sv = [&]() -> boost::optional<PathVertex>
                {
                    // Previous & two before vertex
                    const auto* pv = &vertices.back().sv.get();
                    const auto* ppv = vertices.size() > 1 ? &vertices[vertices.size() - 2].sv.get() : nullptr;

                    // Sample a next direction
                    Vec3 wo;
                    const auto wi = ppv ? Math::Normalize(ppv->geom.p - pv->geom.p) : Vec3();
                    if (step == 1)
                    {
                        wo = initWo;
                    }
                    else
                    {
                        pv->primitive->SampleDirection(rng->Next2D(), rng->Next(), pv->type, pv->geom, wi, wo);
                    }
                    const auto f = pv->primitive->EvaluateDirection(pv->geom, pv->type, wi, wo, transDir, false);
                    if (f.Black())
                    {
                        return boost::none;
                    }

                    // Intersection query
                    Ray ray = { pv->geom.p, wo };
                    Intersection isect;
                    if (!scene->Intersect(ray, isect))
                    {
                        return boost::none;
                    }

                    // Create vertex
                    PathVertex v;
                    v.geom = isect.geom;
                    v.primitive = isect.primitive;
                    v.type = isect.primitive->Type() & ~SurfaceInteractionType::Emitter;

                    return v;
                }();

                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Sample a vertex with direct emitter sampling

                const auto direct = [&]() -> boost::optional<PathVertex>
                {
                    PathVertex v;
                    const auto& pv = vertices.back().sv;

                    // Sample a emitter
                    v.type = transDir == TransportDirection::LE ? SurfaceInteractionType::E : SurfaceInteractionType::L;
                    v.primitive = scene->SampleEmitter(v.type, rng->Next());

                    // Sample a position on the emitter
                    v.primitive->SamplePositionGivenPreviousPosition(rng->Next2D(), pv->geom, v.geom);

                    // Check visibility
                    if (!scene->Visible(pv->geom.p, v.geom.p))
                    {
                        return boost::none;
                    }

                    return v;
                }();

                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Add a vertex

                if (sv || direct)
                {
                    SubpathVertex v;
                    v.sv = sv;
                    v.direct = direct;
                    vertices.push_back(v);
                }

                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Path termination

                if (!sv)
                {
                    break;
                }

                if (sv->geom.infinite)
                {
                    break;
                }

                // TODO: replace it with efficient one
                const Float rrProb = 0.5_f;
                if (rng->Next() > rrProb)
                {
                    break;
                }

                #pragma endregion
            }
        }
    }

};

struct Path
{

    std::vector<PathVertex> vertices;

public:

    #pragma region BDPT path initialization

    auto Connect(const Scene* scene, int s, int t, bool direct, const Subpath& subpathL, const Subpath& subpathE) -> bool
    {
        assert(s > 0 || t > 0);
        vertices.clear();
        if (s == 0 && t > 0)
        {
            if (!direct)
            {
                if (!subpathE.vertices[t - 1].sv)
                {
                    return false;
                }
                if ((subpathE.vertices[t - 1].sv->primitive->Type() & SurfaceInteractionType::L) == 0)
                {
                    return false;
                }
                for (int i = t - 1; i >= 0; i--)
                {
                    assert(subpathE.vertices[i].sv);
                    vertices.push_back(*subpathE.vertices[i].sv);
                }
            }
            else
            {
                if (!subpathE.vertices[t - 1].direct)
                {
                    return false;
                }
                vertices.push_back(*subpathE.vertices[t - 1].direct);
                for (int i = t - 2; i >= 0; i--)
                {
                    assert(subpathE.vertices[i].sv);
                    vertices.push_back(*subpathE.vertices[i].sv);
                }
            }

            vertices.front().type = SurfaceInteractionType::L;
        }
        else if (s > 0 && t == 0)
        {
            if (!direct)
            {
                if (!subpathL.vertices[s - 1].sv)
                {
                    return false;
                }
                if ((subpathL.vertices[s - 1].sv->primitive->Type() & SurfaceInteractionType::E) == 0)
                {
                    return false;
                }
                for (int i = 0; i < s; i++)
                {
                    assert(subpathL.vertices[i].sv);
                    vertices.push_back(*subpathL.vertices[i].sv);
                }
            }
            else
            {
                if (!subpathL.vertices[s - 1].direct)
                {
                    return false;
                }
                for (int i = 0; i < s - 1; i++)
                {
                    assert(subpathL.vertices[i].sv);
                    vertices.push_back(*subpathL.vertices[i].sv);
                }
                vertices.push_back(*subpathL.vertices[s - 1].direct);
            }

            vertices.back().type = SurfaceInteractionType::E;
        }
        else
        {
            assert(s > 0 && t > 0);
            assert(!direct);
            if (!subpathL.vertices[s - 1].sv || !subpathE.vertices[t - 1].sv)
            {
                return false;
            }
            if (subpathL.vertices[s - 1].sv->geom.infinite || subpathE.vertices[t - 1].sv->geom.infinite)
            {
                return false;
            }
            if (!scene->Visible(subpathL.vertices[s - 1].sv->geom.p, subpathE.vertices[t - 1].sv->geom.p))
            {
                return false;
            }
            for (int i = 0; i < s; i++)
            {
                assert(subpathL.vertices[i].sv);
                vertices.push_back(*subpathL.vertices[i].sv);
            }
            for (int i = t - 1; i >= 0; i--)
            {
                assert(subpathE.vertices[i].sv);
                vertices.push_back(*subpathE.vertices[i].sv);
            }
        }
        return true;
    }

    #pragma endregion

public:

    #pragma region BDPT path evaluation

    auto EvaluateContribution(const MISWeight* mis, const Scene* scene, int s, bool direct) const -> SPD
    {
        const auto Cstar = EvaluateUnweightContribution(scene, s, direct);
        //const auto Cstar = EvaluateF(s, direct) / EvaluatePDF(scene, s, direct);
        return Cstar.Black() ? SPD() : Cstar * mis->Evaluate(*this, scene, s, direct);
    }

    auto SelectionPDF(int s, bool direct) const -> Float
    {
        const Float rrProb = 0.5_f;
        const int n = (int)(vertices.size());
        const int t = n - s;
        Float selectionProb = 1;

        // Light subpath
        for (int i = 1; i < s - 1; i++)
        {
            selectionProb *= rrProb;    
        }
        
        // Eye subpath
        for (int i = 1; i < t - 1; i++)
        {
            selectionProb *= rrProb;
        }

        return selectionProb;
    }

    auto RasterPosition() const -> Vec2
    {
        const auto& v = vertices[vertices.size() - 1];
        const auto& vPrev = vertices[vertices.size() - 2];
        Vec2 rasterPos;
        v.primitive->RasterPosition(Math::Normalize(vPrev.geom.p - v.geom.p), v.geom, rasterPos);
        return rasterPos;
    }

    auto EvaluateCst(int s) const -> SPD
    {
        const int n = (int)(vertices.size());
        const int t = n - s;
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

    auto EvaluateF(int s, bool direct) const -> SPD
    {
        const int n = (int)(vertices.size());
        const int t = n - s;
        assert(n >= 2);

        // --------------------------------------------------------------------------------

        SPD fL;
        if (s == 0)
        {
            fL = SPD(1_f);
        }
        else
        {
            {
                const auto* vL  = &vertices[0];
                fL = vL->primitive->EvaluatePosition(vL->geom, false);
            }
            for (int i = 0; i < s - 1; i++)
            {
                const auto* v     = &vertices[i];
                const auto* vPrev = i >= 1 ? &vertices[i - 1] : nullptr;
                const auto* vNext = &vertices[i + 1];
                const auto wi = vPrev ? Math::Normalize(vPrev->geom.p - v->geom.p) : Vec3();
                const auto wo = Math::Normalize(vNext->geom.p - v->geom.p);
                fL *= v->primitive->EvaluateDirection(v->geom, v->type, wi, wo, TransportDirection::LE, t == 0 && i == s - 2 && direct);
                fL *= RenderUtils::GeometryTerm(v->geom, vNext->geom);
            }
        }
        if (fL.Black())
        {
            return SPD();
        }
        
        // --------------------------------------------------------------------------------

        SPD fE;
        if (t == 0)
        {
            fE = SPD(1_f);
        }
        else
        {
            {
                const auto* vE = &vertices[n - 1];
                fE = vE->primitive->EvaluatePosition(vE->geom, false);
            }
            for (int i = n - 1; i > s; i--)
            {
                const auto* v     = &vertices[i];
                const auto* vPrev = &vertices[i - 1];
                const auto* vNext = i < n - 1 ? &vertices[i + 1] : nullptr;
                const auto wi = vNext ? Math::Normalize(vNext->geom.p - v->geom.p) : Vec3();
                const auto wo = Math::Normalize(vPrev->geom.p - v->geom.p);
                fE *= v->primitive->EvaluateDirection(v->geom, v->type, wi, wo, TransportDirection::EL, s == 0 && i == 1 && direct);
                fE *= RenderUtils::GeometryTerm(v->geom, vPrev->geom);
            }
        }
        if (fE.Black())
        {
            return SPD();
        }

        // --------------------------------------------------------------------------------

        const auto cst = EvaluateCst(s);
        if (cst.Black())
        {
            return SPD();
        }

        // --------------------------------------------------------------------------------

        return fL * cst * fE;
    }

    auto EvaluateUnweightContribution(const Scene* scene, int s, bool direct) const -> SPD
    {
        const int n = (int)(vertices.size());
        const int t = n - s;

        // --------------------------------------------------------------------------------

        #pragma region Compute alphaL

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
                const auto fs = v->primitive->EvaluateDirection(v->geom, v->type, wi, wo, TransportDirection::LE, t == 0 && i == s - 2 && direct);
                if (fs.Black()) return SPD();
                alphaL *= 
                    fs /
                    (t == 0 && i == s - 2 && direct
                        ? vNext->primitive->EvaluatePositionGivenPreviousPositionPDF(vNext->geom, v->geom, false).ConvertToProjSA(vNext->geom, v->geom) * scene->EvaluateEmitterPDF(vNext->primitive).v
                        : v->primitive->EvaluateDirectionPDF(v->geom, v->type, wi, wo, false));
            }
        }
        if (alphaL.Black())
        {
            return SPD();
        }

        #pragma endregion

        // --------------------------------------------------------------------------------

        #pragma region Compute alphaE

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
                const auto fs = v->primitive->EvaluateDirection(v->geom, v->type, wi, wo, TransportDirection::EL, s == 0 && i == 1 && direct);
                if (fs.Black()) return SPD();
                alphaE *= 
                    fs /
                    (s == 0 && i == 1 && direct
                        ? vPrev->primitive->EvaluatePositionGivenPreviousPositionPDF(vPrev->geom, v->geom, false).ConvertToProjSA(vPrev->geom, v->geom) * scene->EvaluateEmitterPDF(vPrev->primitive).v
                        : v->primitive->EvaluateDirectionPDF(v->geom, v->type, wi, wo, false));
            }
        }
        if (alphaE.Black())
        {
            return SPD();
        }

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

    auto Samplable(int s, bool direct) const -> bool
    {
        // There is no connection with some cases
        const int n = (int)(vertices.size());
        const int t = n - s;
        if (s > 0 && t > 0 && direct)
        {
            return false;
        }

        // Delta connection with direct light sampling
        if (t == 0 && s > 0 && direct)
        {
            if (vertices[n - 2].primitive->IsDeltaDirection(vertices[n - 2].type))
            {
                return false;
            }
        }
        if (s == 0 && t > 0 && direct)
        {
            if (vertices[1].primitive->IsDeltaDirection(vertices[1].type))
            {
                return false;
            }
        }

        // Delta connection of endpoints
        if (s == 0 && t > 0)
        {
            const auto& v = vertices[0];
            if (v.primitive->IsDeltaPosition(v.type))
            {
                return false;
            }
        }
        else if (s > 0 && t == 0)
        {
            const auto& v = vertices[n - 1];
            if (v.primitive->IsDeltaPosition(v.type))
            {
                return false;
            }
        }
        else if (s > 0 && t > 0)
        {
            const auto* vL = &vertices[s - 1];
            const auto* vE = &vertices[s];
            if (vL->primitive->IsDeltaDirection(vL->type) || vE->primitive->IsDeltaDirection(vE->type))
            {
                return false;
            }
        }

        return true;
    }

    auto EvaluatePDF(const Scene* scene, int s, bool direct) const -> PDFVal
    {
        if (!Samplable(s, direct))
        {
            return PDFVal(PDFMeasure::ProdArea, 0_f);
        }

        // Otherwise the path can be generated with the given strategy (s,t)
        // so p_{s,t} can be safely evaluated.
        PDFVal pdf(PDFMeasure::ProdArea, 1_f);
        const int n = (int)(vertices.size());
        const int t = n - s;
        if (s > 0)
        {
            pdf *= vertices[0].primitive->EvaluatePositionGivenDirectionPDF(vertices[0].geom, Math::Normalize(vertices[1].geom.p - vertices[0].geom.p), false) * scene->EvaluateEmitterPDF(vertices[0].primitive).v;
            for (int i = 0; i < s - 1; i++)
            {
                const auto* vi = &vertices[i];
                const auto* vip = i - 1 >= 0 ? &vertices[i - 1] : nullptr;
                const auto* vin = &vertices[i + 1];
                if (t == 0 && i == s - 2 && direct)
                {
                    pdf *= vin->primitive->EvaluatePositionGivenPreviousPositionPDF(vin->geom, vi->geom, false) * scene->EvaluateEmitterPDF(vin->primitive).v;
                }
                else
                {
                    pdf *= vi->primitive->EvaluateDirectionPDF(vi->geom, vi->type, vip ? Math::Normalize(vip->geom.p - vi->geom.p) : Vec3(), Math::Normalize(vin->geom.p - vi->geom.p), false).ConvertToArea(vi->geom, vin->geom);
                }
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
                if (s == 0 && i == s + 1 && direct)
                {
                    pdf *= vip->primitive->EvaluatePositionGivenPreviousPositionPDF(vip->geom, vi->geom, false) * scene->EvaluateEmitterPDF(vip->primitive).v;
                }
                else
                {
                    pdf *= vi->primitive->EvaluateDirectionPDF(vi->geom, vi->type, vin ? Math::Normalize(vin->geom.p - vi->geom.p) : Vec3(), Math::Normalize(vip->geom.p - vi->geom.p), false).ConvertToArea(vi->geom, vip->geom);
                }
            }
        }

        return pdf;
    }

    #pragma endregion

};

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region MIS weights implementations

class MISWeight_Simple : public MISWeight
{
public:

    LM_IMPL_CLASS(MISWeight_Simple, MISWeight);

public:

    LM_IMPL_F(Evaluate) = [this](const Path& path, const Scene* scene, int s_, bool direct_) -> Float
    {
        const int n = (int)(path.vertices.size());
        int nonzero = 0;

        for (int s = 0; s <= n; s++)
        {
            for (int d = 0; d < 2; d++)
            {
                bool direct = d == 1;
                const auto t = n - s;
                if (s > 0 && t > 0 && direct)
                {
                    continue;
                }
                if (path.EvaluatePDF(scene, s, direct).v > 0_f)
                {
                    nonzero++;
                }
            }
        }

        assert(nonzero != 0);
        return 1_f / nonzero;
    };

};

class MISWeight_PowerHeuristics : public MISWeight
{
public:

    LM_IMPL_CLASS(MISWeight_PowerHeuristics, MISWeight);

public:

    LM_IMPL_F(Evaluate) = [this](const Path& path, const Scene* scene, int s_, bool direct_) -> Float
    {
        const int n = static_cast<int>(path.vertices.size());
        const auto ps = path.EvaluatePDF(scene, s_, direct_);
        assert(ps > 0_f);

        Float invWeight = 0;
        for (int s = 0; s <= n; s++)
        {
            for (int d = 0; d < 2; d++)
            {
                bool direct = d == 1;
                const auto t = n - s;
                if (s > 0 && t > 0 && direct)
                {
                    continue;
                }
                const auto pi = path.EvaluatePDF(scene, s, direct);
                if (pi > 0_f)
                {
                    const auto r = pi.v / ps.v;
                    invWeight += r * r;
                }
            }
        }

        return 1_f / invWeight;
    };

};

LM_COMPONENT_REGISTER_IMPL(MISWeight_Simple,          "misweight::simple");
LM_COMPONENT_REGISTER_IMPL(MISWeight_PowerHeuristics, "misweight::powerheuristics");

#pragma endregion

// --------------------------------------------------------------------------------

struct Strategy
{
    int s;
    int t;
    int d;
    bool operator==(const Strategy& o) const
    {
        return (s == o.s && t == o.t && d == o.d);
    }
};

struct StrategyHash
{
    const int N = 100;
    auto operator()(const Strategy& v) const -> size_t
    {
        return (v.s * N + v.t) * 2 + v.d;
    }
};

/*!
    \brief BDPT renderer.
    Implements bidirectional path tracing.
*/
class Renderer_BDPT final : public Renderer
{
public:

    LM_IMPL_CLASS(Renderer_BDPT, Renderer);

private:

    int maxNumVertices_;
    int minNumVertices_;
    Scheduler::UniquePtr sched_ = ComponentFactory::Create<Scheduler>();
    MISWeight::UniquePtr mis_{ nullptr, nullptr };

public:

    LM_IMPL_F(Initialize) = [this](const PropertyNode* prop) -> bool
    {
        sched_->Load(prop);
        maxNumVertices_ = prop->ChildAs("max_num_vertices", -1);
        minNumVertices_ = prop->ChildAs("min_num_vertices", 0);
        mis_ = ComponentFactory::Create<MISWeight>("misweight::" + prop->ChildAs<std::string>("mis", "powerheuristics"));
        return true;
    };

    LM_IMPL_F(Render) = [this](const Scene* scene, Random* initRng, const std::string& outputPath) -> void
    {
        #if LM_COMPILER_CLANG
        tbb::enumerable_thread_specific<Subpath> subpathL_, subpathE_;
        tbb::enumerable_thread_specific<Path> path_;
        #else
        static thread_local Subpath subpathL, subpathE;
        static thread_local Path path;
        #endif

        // --------------------------------------------------------------------------------

        #if LM_BDPT_DEBUG
        std::vector<Film::UniquePtr> strategyFilms1;
        std::vector<Film::UniquePtr> strategyFilms2;
        std::unordered_map<Strategy, size_t, StrategyHash> strategyFilmMap;
        std::mutex strategyFilmMutex;
        #endif

        // --------------------------------------------------------------------------------

        auto* film = static_cast<const Sensor*>(scene->GetSensor()->emitter)->GetFilm();
        const auto processedSamples = sched_->Process(scene, film, initRng, [&](Film* film, Random* rng)
        {
            #if LM_COMPILER_CLANG
            auto& subpathL = subpathL_.local();
            auto& subpathE = subpathE_.local();
            auto& path     = path_.local();
            #endif

            // --------------------------------------------------------------------------------

            #pragma region Sample subpaths

            subpathL.Sample(scene, rng, TransportDirection::LE, maxNumVertices_);
            subpathE.Sample(scene, rng, TransportDirection::EL, maxNumVertices_);

            #pragma endregion 

            // --------------------------------------------------------------------------------

            #pragma region Evaluate path combinations

            const int nL = static_cast<int>(subpathL.vertices.size());
            const int nE = static_cast<int>(subpathE.vertices.size());
            for (int n = 2; n <= nE + nL; n++)
            {
                if (maxNumVertices_ != -1 && (n > maxNumVertices_ || n < minNumVertices_))
                {
                    continue;
                }

                // --------------------------------------------------------------------------------

                const int minS = Math::Max(0, n - nE);
                const int maxS = Math::Min(nL, n);
                for (int s = minS; s <= maxS; s++)
                {
                    for (int d = 0; d < 2; d++)
                    {
                        #pragma region Exclude some combination of paths to control number of strategies

                        const bool direct = d == 1;
                        const int t = n - s;

                        // Only direct connection with the cases with s=0 and t=0
                        if (s > 0 && t > 0 && direct)
                        {
                            continue;
                        }

                        #pragma endregion

                        // --------------------------------------------------------------------------------

                        #pragma region Connect subpaths & create fullpath

                        if (!path.Connect(scene, s, t, direct, subpathL, subpathE))
                        {
                            continue;
                        }

                        #pragma endregion

                        // --------------------------------------------------------------------------------

                        #pragma region Evaluate contribution

                        const auto C = path.EvaluateContribution(mis_.get(), scene, s, direct) / path.SelectionPDF(s, direct);
                        if (C.Black())
                        {
                            continue;
                        }

                        #pragma endregion

                        // --------------------------------------------------------------------------------

                        #pragma region Accumulate to film

                        film->Splat(path.RasterPosition(), C);

                        #if LM_BDPT_DEBUG
                        {
                            const auto Cstar = path.EvaluateUnweightContribution(scene, s, direct) / path.SelectionPDF(s, direct);
                            std::unique_lock<std::mutex> lock(strategyFilmMutex);
                            Strategy strategy{ s, t, d };
                            if (strategyFilmMap.find(strategy) == strategyFilmMap.end())
                            {
                                strategyFilms1.push_back(ComponentFactory::Clone<Film>(film));
                                strategyFilms2.push_back(ComponentFactory::Clone<Film>(film));
                                strategyFilms1.back()->Clear();
                                strategyFilms2.back()->Clear();
                                strategyFilmMap[strategy] = strategyFilms1.size()-1;
                            }
                            strategyFilms1[strategyFilmMap[strategy]]->Splat(path.RasterPosition(), C);
                            strategyFilms2[strategyFilmMap[strategy]]->Splat(path.RasterPosition(), Cstar);
                        }
                        #endif

                        #pragma endregion
                    }
                }
            }

            #pragma endregion
        });

        // --------------------------------------------------------------------------------

        #if LM_BDPT_DEBUG
        for (const auto& kv : strategyFilmMap)
        {
            const auto* f1 = strategyFilms1[kv.second].get();
            const auto* f2 = strategyFilms2[kv.second].get();
            f1->Rescale((Float)(f1->Width() * f1->Height()) / processedSamples);
            f2->Rescale((Float)(f2->Width() * f2->Height()) / processedSamples);
            f1->Save(boost::str(boost::format("bdpt_f1_n%02d_s%02d_t%02d_d%d") % (kv.first.s + kv.first.t) % kv.first.s % kv.first.t % kv.first.d));
            f2->Save(boost::str(boost::format("bdpt_f2_n%02d_s%02d_t%02d_d%d") % (kv.first.s + kv.first.t) % kv.first.s % kv.first.t % kv.first.d));
        }
        #else
        LM_UNUSED(processedSamples);
        #endif

        // --------------------------------------------------------------------------------

        #pragma region Save image
        {
            LM_LOG_INFO("Saving image");
            LM_LOG_INDENTER();
            film->Save(outputPath);
        }
        #pragma endregion
    };

};

LM_COMPONENT_REGISTER_IMPL(Renderer_BDPT, "renderer::bdpt");

LM_NAMESPACE_END

