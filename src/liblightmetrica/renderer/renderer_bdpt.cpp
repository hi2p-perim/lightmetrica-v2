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

LM_NAMESPACE_BEGIN

struct PathVertex
{

    // Surface interaction type
    int type = SurfaceInteractionType::None;

    // Surface geometry
    SurfaceGeometry geom;

    // Primitive
    const Primitive* primitive = nullptr;

    // Specifies the path vertex is sampled with direct emitter sampling.
    // This parameter is utilized only with subpaths.
    bool direct = false;
                                
};

struct Path
{

    std::vector<PathVertex> vertices;

public:

    #pragma region BDPT path initialization

    auto SampleSubpath(const Scene* scene, Random* rng, TransportDirection transDir, int maxPathVertices) -> void
    {
        vertices.clear();

        // --------------------------------------------------------------------------------

        #pragma region Sample intermediate vertex

        Vec3 initWo;
        for (int step = 0; maxPathVertices == -1 || step < maxPathVertices; step++)
        {
            if (step == 0)
            {
                #pragma region Sample initial vertex
        
                PathVertex v;

                // Sample an emitter
                v.type = transDir == TransportDirection::LE ? SurfaceInteractionType::L : SurfaceInteractionType::E;
                v.primitive = scene->SampleEmitter(v.type, rng->Next());

                // Sample a position on the emitter and initial ray direction
                v.primitive->emitter->SamplePositionAndDirection(rng->Next2D(), rng->Next2D(), v.geom, initWo);

                // Create a vertex
                vertices.push_back(v);

                #pragma endregion
            }
            else
            {
                #pragma region Sample intermediate vertex

                // Previous & two before vertex
                const auto* pv = &vertices.back();
                const auto* ppv = vertices.size() > 1 ? &vertices[vertices.size() - 2] : nullptr;

                // Sample a next direction
                Vec3 wo;
                const auto wi = ppv ? Math::Normalize(ppv->geom.p - pv->geom.p) : Vec3();
                if (step == 1)
                {
                    wo = initWo;
                }
                else
                {
                    pv->primitive->surface->SampleDirection(rng->Next2D(), rng->Next(), pv->type, pv->geom, wi, wo);
                }
                const auto f = pv->primitive->surface->EvaluateDirection(pv->geom, pv->type, wi, wo, transDir, false);
                if (f.Black())
                {
                    break;
                }

                // Intersection query
                Ray ray = { pv->geom.p, wo };
                Intersection isect;
                if (!scene->Intersect(ray, isect))
                {
                    break;
                }

                // Set vertex information
                PathVertex v;
                v.geom = isect.geom;
                v.primitive = isect.primitive;
                v.type = isect.primitive->surface->Type() & ~SurfaceInteractionType::Emitter;
                
                // Add a vertex
                vertices.push_back(v);

                // Terminate if intersected with infinite light sources
                if (isect.geom.infinite)
                {
                    break;
                }

                #pragma endregion
            }

            // --------------------------------------------------------------------------------

            #pragma region Path termination

            // TODO: replace it with efficient one
            const Float rrProb = 0.5_f;
            if (rng->Next() > rrProb)
            {
                #pragma region Add last vertex with direct light sampling

                const auto& pv = vertices.back();
                PathVertex v;
                v.direct = true;
                v.type = transDir == TransportDirection::LE ? SurfaceInteractionType::E : SurfaceInteractionType::L;

                // --------------------------------------------------------------------------------

                #pragma region Sample a emitter

                v.primitive = scene->SampleEmitter(v.type, rng->Next());
                const auto pdfSel = scene->EvaluateEmitterPDF(v.primitive);
                assert(pdfSel.v > 0);

                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Sample a position on the emitter

                SurfaceGeometry geom;
                v.primitive->emitter->SamplePositionGivenPreviousPosition(rng->Next2D(), pv.geom, geom);
                const auto pdfP = v.primitive->emitter->EvaluatePositionGivenPreviousPositionPDF(geom, pv.geom, false);
                assert(pdfP.v > 0);

                #pragma endregion

                // --------------------------------------------------------------------------------

                vertices.push_back(v);
                
                #pragma endregion

                break;
            }

            #pragma endregion
        }

        #pragma endregion
    }

    auto Connect(const Scene* scene, int s, int t, const Path& subpathL, const Path& subpathE) -> bool
    {
        assert(s > 0 || t > 0);

        vertices.clear();

        if (s == 0 && t > 0)
        {
            if ((subpathE.vertices[t - 1].primitive->surface->Type() & SurfaceInteractionType::L) == 0)
            {
                return false;
            }
            for (int i = t - 1; i >= 0; i--)
            {
                vertices.push_back(subpathE.vertices[i]);
            }
            vertices.front().type = SurfaceInteractionType::L;
        }
        else if (s > 0 && t == 0)
        {
            if ((subpathL.vertices[s - 1].primitive->surface->Type() & SurfaceInteractionType::E) == 0)
            {
                return false;
            }
            for (int i = 0; i < s; i++)
            {
                vertices.push_back(subpathL.vertices[i]);
            }
            vertices.back().type = SurfaceInteractionType::E;
        }
        else
        {
            assert(s > 0 && t > 0);
            if (subpathL.vertices[s - 1].geom.infinite || subpathE.vertices[t - 1].geom.infinite)
            {
                return false;
            }
            if (subpathL.vertices[s - 1].direct || subpathL.vertices[t - 1].direct)
            {
                return false;
            }
            if (!scene->Visible(subpathL.vertices[s - 1].geom.p, subpathE.vertices[t - 1].geom.p))
            {
                return false;
            }
            for (int i = 0; i < s; i++)
            {
                vertices.push_back(subpathL.vertices[i]);
            }
            for (int i = t - 1; i >= 0; i--)
            {
                vertices.push_back(subpathE.vertices[i]);
            }
        }

        return true;
    }

    #pragma endregion

public:

    #pragma region BDPT path evaluation

    auto EvaluateContribution(const Scene* scene, int s, bool direct) const -> SPD
    {
        const auto Cstar = EvaluateUnweightContribution(scene, s, direct);
        //return Cstar.Black() ? SPD() : Cstar * EvaluatePowerHeuristicsMISWeightOpt(scene, s);
        return Cstar.Black() ? SPD() : Cstar * EvaluateSimpleMISWeight(scene);
    }

    auto SelectionPDF(int s, bool direct) const -> Float
    {
        const Float rrProb = 0.5_f;
        const int n = (int)(vertices.size());
        const int t = n - s;
        Float selectionProb = 1;

        // Light subpath
        for (int i = 0; i < s - 2; i++)
        {
            selectionProb *= rrProb;    
        }
        if (s >= 2)
        {
            if (vertices[s - 1].direct && direct)
            {
                selectionProb *= (1_f - rrProb);
            }
            else
            {
                selectionProb *= rrProb;
            }
        }
        
        // Eye subpath
        for (int i = 0; i < t - 2; i++)
        {
            selectionProb *= rrProb;
        }
        if (t >= 2)
        {
            if (vertices[s].direct && direct)
            {
                selectionProb *= (1_f - rrProb);
            }
            else
            {
                selectionProb *= rrProb;
            }
        }

        return selectionProb;
    }

    auto RasterPosition() const -> Vec2
    {
        const auto& v = vertices[vertices.size() - 1];
        const auto& vPrev = vertices[vertices.size() - 2];
        Vec2 rasterPos;
        v.primitive->sensor->RasterPosition(Math::Normalize(vPrev.geom.p - v.geom.p), v.geom, rasterPos);
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
            cst = v.primitive->surface->EvaluatePosition(v.geom, true) * v.primitive->surface->EvaluateDirection(v.geom, v.type, Vec3(), Math::Normalize(vNext.geom.p - v.geom.p), TransportDirection::EL, true);
        }
        else if (s > 0 && t == 0)
        {
            const auto& v = vertices[n - 1];
            const auto& vPrev = vertices[n - 2];
            cst = v.primitive->surface->EvaluatePosition(v.geom, true) * v.primitive->surface->EvaluateDirection(v.geom, v.type, Vec3(), Math::Normalize(vPrev.geom.p - v.geom.p), TransportDirection::LE, true);
        }
        else if (s > 0 && t > 0)
        {
            const auto* vL = &vertices[s - 1];
            const auto* vE = &vertices[s];
            const auto* vLPrev = s - 2 >= 0 ? &vertices[s - 2] : nullptr;
            const auto* vENext = s + 1 < n ? &vertices[s + 1] : nullptr;
            const auto fsL = vL->primitive->surface->EvaluateDirection(vL->geom, vL->type, vLPrev ? Math::Normalize(vLPrev->geom.p - vL->geom.p) : Vec3(), Math::Normalize(vE->geom.p - vL->geom.p), TransportDirection::LE, true);
            const auto fsE = vE->primitive->surface->EvaluateDirection(vE->geom, vE->type, vENext ? Math::Normalize(vENext->geom.p - vE->geom.p) : Vec3(), Math::Normalize(vL->geom.p - vE->geom.p), TransportDirection::EL, true);
            const Float G = RenderUtils::GeometryTerm(vL->geom, vE->geom);
            cst = fsL * G * fsE;
        }

        return cst;
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
            alphaL = SPD(1);
        }
        else
        {
            {
                const auto* v = &vertices[0];
                const auto* vNext = &vertices[1];
                alphaL =
                    v->primitive->surface->EvaluatePosition(v->geom, false) /
                    v->primitive->surface->EvaluatePositionGivenDirectionPDF(v->geom, Math::Normalize(vNext->geom.p - v->geom.p), false) /
                    scene->EvaluateEmitterPDF(v->primitive).v;
            }
            for (int i = 0; i < s - 1; i++)
            {
                const auto* v     = &vertices[i];
                const auto* vPrev = i >= 1 ? &vertices[i - 1] : nullptr;
                const auto* vNext = &vertices[i + 1];
                const auto wi = vPrev ? Math::Normalize(vPrev->geom.p - v->geom.p) : Vec3();
                const auto wo = Math::Normalize(vNext->geom.p - v->geom.p);
                alphaL *= 
                    v->primitive->surface->EvaluateDirection(v->geom, v->type, wi, wo, TransportDirection::LE, false) /
                    (vNext->direct && direct
                        ? vNext->primitive->surface->EvaluatePositionGivenPreviousPositionPDF(vNext->geom, v->geom, false).ConvertToProjSA(vNext->geom, v->geom)
                        : v->primitive->surface->EvaluateDirectionPDF(v->geom, v->type, wi, wo, false));
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
            alphaE = SPD(1);
        }
        else
        {
            {
                const auto* v = &vertices[n - 1];
                const auto* vPrev = &vertices[n - 2];
                alphaE =
                    v->primitive->surface->EvaluatePosition(v->geom, false) /
                    v->primitive->surface->EvaluatePositionGivenDirectionPDF(v->geom, Math::Normalize(vPrev->geom.p - v->geom.p), false) /
                    scene->EvaluateEmitterPDF(v->primitive).v;
            }
            for (int i = n - 1; i > s; i--)
            {
                const auto* v = &vertices[i];
                const auto* vPrev = &vertices[i - 1];
                const auto* vNext = i < n - 1 ? &vertices[i + 1] : nullptr;
                const auto wi = vNext ? Math::Normalize(vNext->geom.p - v->geom.p) : Vec3();
                const auto wo = Math::Normalize(vPrev->geom.p - v->geom.p);
                alphaE *= 
                    v->primitive->surface->EvaluateDirection(v->geom, v->type, wi, wo, TransportDirection::EL, false) /
                    (vPrev->direct && direct
                        ? vPrev->primitive->surface->EvaluatePositionGivenPreviousPositionPDF(vPrev->geom, v->geom, false).ConvertToProjSA(vPrev->geom, v->geom)
                        : v->primitive->surface->EvaluateDirectionPDF(v->geom, v->type, wi, wo, false));
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

    auto EvaluateSimpleMISWeight(const Scene* scene) const -> Float
    {
        const int n = (int)(vertices.size());
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
                if (EvaluatePDF(scene, s, direct).v > 0_f)
                {
                    nonzero++;
                }
            }
        }

        assert(nonzero != 0);
        return 1_f / nonzero;
    }

#if 0

    auto EvaluatePowerHeuristicsMISWeightOpt(const Scene* scene, int s) const -> Float
    {
        Float invWeight = 0;
        const int n = static_cast<int>(vertices.size());
        const Float ps = EvaluatePDF(scene, s);
        assert(ps > 0);

        for (int i = 0; i <= n; i++)
        {
            const auto pi = EvaluatePDF(scene, i);
            if (pi > 0)
            {
                const auto r = pi / ps;
                invWeight += r * r;
            }
        }

        return 1_f / invWeight;
    }

    auto EvaluateMISWeight(const Scene* scene, int s) const -> Float
    {
        Float piDivps;
        bool prevPDFIsZero;
        Float invWeight = 1;
        const int n = static_cast<int>(vertices.size());

        const Float ps = EvaluatePDF(scene, s);
        assert(ps > 0);

        piDivps = 1;
        prevPDFIsZero = false;
        for (int i = s - 1; i >= 0; i--)
        {
            if (EvaluateCst(i).Black())
            {
                prevPDFIsZero = true;
                continue;
            }

            if (prevPDFIsZero)
            {
                piDivps = EvaluatePDF(scene, i) / ps;
                prevPDFIsZero = false;
            }
            else
            {
                const Float ratio = EvaluatePDFRatio(scene, i);
                if (ratio == 0)
                {
                    break;
                }
                piDivps *= 1_f / ratio;
            }

            invWeight += piDivps * piDivps;
        }

        piDivps = 1;
        prevPDFIsZero = false;
        for (int i = s; i < n; i++)
        {
            if (EvaluateCst(i+1).Black())
            {
                prevPDFIsZero = true;
                continue;
            }

            if (prevPDFIsZero)
            {
                piDivps = EvaluatePDF(scene, i) / ps;
                prevPDFIsZero = false;
            }
            else
            {
                const Float ratio = EvaluatePDFRatio(scene, i);
                if (ratio == 0)
                {
                    break;
                }
                piDivps *= ratio;
            }

            invWeight += piDivps * piDivps;
        }

        return 1_f / invWeight;
    }

    auto EvaluatePDFRatio(const Scene* scene, int i) const -> Float
    {
        const int n = static_cast<int>(vertices.size());

        if (i == 0)
        {
            const auto* x0 = &vertices[0];
            const auto* x1 = &vertices[1];
            const auto* x2 = n > 2 ? &vertices[2] : nullptr;
            const Float G = RenderUtils::GeometryTerm(x0->geom, x1->geom);
            const Float pAx0 = x0->primitive->EvaluatePositionPDF(x0->geom, false) * scene->EvaluateEmitterPDF(x0->primitive);
            const Float pDx1x0 = x1->primitive->EvaluateDirectionPDF(x1->geom, x1->type, x2 ? Math::Normalize(x2->geom.p - x1->geom.p) : Vec3(), Math::Normalize(x0->geom.p - x1->geom.p), false);
            return pAx0 / pDx1x0 / G;
        }

        if (i == n - 1)
        {
            const auto* xnp = &vertices[n - 1];
            const auto* xnp2 = &vertices[n - 2];
            const auto* xnp3 = n > 2 ? &vertices[n - 3] : nullptr;
            const Float G = RenderUtils::GeometryTerm(xnp->geom, xnp2->geom);
            const Float pAxnp = xnp->primitive->EvaluatePositionPDF(xnp->geom, false) * scene->EvaluateEmitterPDF(xnp->primitive);
            const Float pDxnp2xnp = xnp2->primitive->EvaluateDirectionPDF(xnp2->geom, xnp2->type, xnp3 ? Math::Normalize(xnp3->geom.p - xnp2->geom.p) : Vec3(), Math::Normalize(xnp->geom.p - xnp2->geom.p), false);
            return pDxnp2xnp * G / pAxnp;
        }

        {
            const auto* xi = &vertices[i];
            const auto* xin = &vertices[i + 1];
            const auto* xip = &vertices[i - 1];
            const auto* xin2 = i + 2 < n ? &vertices[i + 2] : nullptr;
            const auto* xip2 = i - 2 >= 0 ? &vertices[i - 2] : nullptr;
            const Float Gxipxi = RenderUtils::GeometryTerm(xip->geom, xi->geom);
            const Float Gxinxi = RenderUtils::GeometryTerm(xin->geom, xi->geom);
            const Float pDxipxi = xip->primitive->EvaluateDirectionPDF(xip->geom, xip->type, xip2 ? Math::Normalize(xip2->geom.p - xip->geom.p) : Vec3(), Math::Normalize(xi->geom.p - xip->geom.p), false);
            const Float pDxinxi = xin->primitive->EvaluateDirectionPDF(xin->geom, xin->type, xin2 ? Math::Normalize(xin2->geom.p - xin->geom.p) : Vec3(), Math::Normalize(xi->geom.p - xin->geom.p), false);
            return pDxipxi * Gxipxi / pDxinxi / Gxinxi;
        }
    }

#endif

    auto EvaluatePDF(const Scene* scene, int s, bool direct) const -> PDFVal
    {
        // Cases with p_{s,t}(x) = 0
        // i.e. the strategy (s,t) cannot generate the path
        // This condition is equivalent to c_{s,t}(x) = 0
        if (EvaluateCst(s).Black())
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
            pdf *= vertices[0].primitive->emitter->EvaluatePositionGivenDirectionPDF(vertices[0].geom, Math::Normalize(vertices[1].geom.p - vertices[0].geom.p), false) * scene->EvaluateEmitterPDF(vertices[0].primitive).v;
            for (int i = 0; i < s - 1; i++)
            {
                const auto* vi = &vertices[i];
                const auto* vip = i - 1 >= 0 ? &vertices[i - 1] : nullptr;
                const auto* vin = &vertices[i + 1];
                if (vin->direct && direct)
                {
                    pdf *= vin->primitive->surface->EvaluatePositionGivenPreviousPositionPDF(vin->geom, vi->geom, false);
                }
                else
                {
                    pdf *= vi->primitive->surface->EvaluateDirectionPDF(vi->geom, vi->type, vip ? Math::Normalize(vip->geom.p - vi->geom.p) : Vec3(), Math::Normalize(vin->geom.p - vi->geom.p), false).ConvertToArea(vi->geom, vin->geom);
                }
            }
        }
        if (t > 0)
        {
            pdf *= vertices[n - 1].primitive->emitter->EvaluatePositionGivenDirectionPDF(vertices[n - 1].geom, Math::Normalize(vertices[n - 2].geom.p - vertices[n - 1].geom.p), false) * scene->EvaluateEmitterPDF(vertices[n - 1].primitive).v;
            for (int i = n - 1; i >= s + 1; i--)
            {
                const auto* vi = &vertices[i];
                const auto* vip = &vertices[i - 1];
                const auto* vin = i + 1 < n ? &vertices[i + 1] : nullptr;
                if (vip->direct && direct)
                {
                    pdf *= vip->primitive->surface->EvaluatePositionGivenPreviousPositionPDF(vip->geom, vi->geom, false);
                }
                else
                {
                    pdf *= vi->primitive->surface->EvaluateDirectionPDF(vi->geom, vi->type, vin ? Math::Normalize(vin->geom.p - vi->geom.p) : Vec3(), Math::Normalize(vip->geom.p - vi->geom.p), false).ConvertToArea(vi->geom, vip->geom);
                }
            }
        }

        return pdf;
    }

    #pragma endregion

};

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

class Renderer_BDPT final : public Renderer
{
public:

    LM_IMPL_CLASS(Renderer_BDPT, Renderer);

public:

    Renderer_BDPT()
        : sched_(ComponentFactory::Create<Scheduler>())
    {}

public:

    LM_IMPL_F(Initialize) = [this](const PropertyNode* prop) -> bool
    {
        sched_->Load(prop);
        maxNumVertices_ = prop->Child("max_num_vertices")->As<int>();
        //s_ = prop->ChildAs<int>("s", -1);
        //t_ = prop->ChildAs<int>("t", -1);
        //n_ = prop->ChildAs<int>("n", -1);
        //d_ = prop->ChildAs<int>("d", -1);
        return true;
    };

    LM_IMPL_F(Render) = [this](const Scene* scene, Film* film) -> void
    {
        Random initRng;
        #if LM_DEBUG_MODE
        initRng.SetSeed(1008556906);
        #else
        initRng.SetSeed(static_cast<unsigned int>(std::time(nullptr)));
        #endif

        #if LM_COMPILER_CLANG
        tbb::enumerable_thread_specific<Path> subpathL_, subpathE_;
        tbb::enumerable_thread_specific<Path> path_;
        #else
        static thread_local Path subpathL, subpathE;
        static thread_local Path path;
        #endif

        // --------------------------------------------------------------------------------

        std::vector<Film::UniquePtr> strategyFilms1;
        std::vector<Film::UniquePtr> strategyFilms2;
        std::unordered_map<Strategy, size_t, StrategyHash> strategyFilmMap;
        std::mutex strategyFilmMutex;

        // --------------------------------------------------------------------------------

        sched_->Process(scene, film, &initRng, [&](const Scene* scene, Film* film, Random* rng)
        {
            #if LM_COMPILER_CLANG
            auto& subpathL = subpathL_.local();
            auto& subpathE = subpathE_.local();
            auto& path     = path_.local();
            #endif

            // --------------------------------------------------------------------------------

            #pragma region Sample subpaths

            subpathL.vertices.clear();
            subpathE.vertices.clear();
            subpathL.SampleSubpath(scene, rng, TransportDirection::LE, maxNumVertices_);
            subpathE.SampleSubpath(scene, rng, TransportDirection::EL, maxNumVertices_);

            #pragma endregion 

            // --------------------------------------------------------------------------------

            #pragma region Evaluate path combinations

            const int nL = static_cast<int>(subpathL.vertices.size());
            const int nE = static_cast<int>(subpathE.vertices.size());
            for (int n = 2; n <= nE + nL; n++)
            {
                if (maxNumVertices_ != -1 && n > maxNumVertices_)
                {
                    continue;
                }

                // --------------------------------------------------------------------------------

                const int minS = Math::Max(0, n - nE);
                const int maxS = Math::Min(nL, n);
                for (int s = minS; s <= maxS; s++)
                {
                    #pragma region Connect subpaths & create fullpath

                    const int t = n - s;
                    if (!path.Connect(scene, s, t, subpathL, subpathE))
                    {
                        continue;
                    }

                    #pragma endregion

                    // --------------------------------------------------------------------------------

                    for (int d = 0; d < 2; d++)
                    {
                        #pragma region Exclude some combination of paths to control number of strategies

                        const bool direct = d == 1;

                        // Only direct connection with the cases with s=0 and t=0
                        if (s > 0 && t > 0 && direct)
                        {
                            continue;
                        }

                        // Last sampled vertex is not direct light sampling
                        if (t == 0 && direct && !path.vertices[s - 1].direct)
                        {
                            continue;
                        }
                        if (s == 0 && direct && !path.vertices[s].direct)
                        {
                            continue;
                        }

                        #pragma endregion

                        // --------------------------------------------------------------------------------

                        #pragma region Evaluate contribution

                        const auto C = path.EvaluateContribution(scene, s, direct) / path.SelectionPDF(s, direct);
                        if (C.Black())
                        {
                            continue;
                        }

                        const auto Cstar = path.EvaluateUnweightContribution(scene, s, direct) / path.SelectionPDF(s, direct);

                        #pragma endregion

                        // --------------------------------------------------------------------------------

                        #pragma region Accumulate to film

                        film->Splat(path.RasterPosition(), C);

                        if (s == 0 && t == 2 && d == 0)
                        {
                            if (std::abs(Cstar.v.x - 2_f) > Math::EpsLarge())
                            {
                                __debugbreak();
                            }
                        }

                        {
                            std::unique_lock<std::mutex> lock(strategyFilmMutex);
                            Strategy strategy{ s, t, d };
                            if (strategyFilmMap.find(strategy) == strategyFilmMap.end())
                            {
                                strategyFilms1.push_back(ComponentFactory::Clone<Film>(film));
                                strategyFilms2.push_back(ComponentFactory::Clone<Film>(film));
                                strategyFilmMap[strategy] = strategyFilms1.size()-1;
                            }
                            strategyFilms1[strategyFilmMap[strategy]]->Splat(path.RasterPosition(), C);
                            strategyFilms2[strategyFilmMap[strategy]]->Splat(path.RasterPosition(), Cstar);
                        }

                        #pragma endregion
                    }
                }
            }

            #pragma endregion
        });

        // --------------------------------------------------------------------------------

        for (const auto& kv : strategyFilmMap)
        {
            const auto* f1 = strategyFilms1[kv.second].get();
            const auto* f2 = strategyFilms2[kv.second].get();
            f1->Rescale((Float)(f1->Width() * f1->Height()) / sched_->GetNumSamples());
            f2->Rescale((Float)(f2->Width() * f2->Height()) / sched_->GetNumSamples());
            f1->Save(boost::str(boost::format("f1_s%02d_t%02d_d%d") % kv.first.s % kv.first.t % kv.first.d));
            f2->Save(boost::str(boost::format("f2_s%02d_t%02d_d%d") % kv.first.s % kv.first.t % kv.first.d));
        }
    };

private:

    int maxNumVertices_;
    Scheduler::UniquePtr sched_;
    //int s_;
    //int t_;
    //int n_;
    //int d_;

};

LM_COMPONENT_REGISTER_IMPL(Renderer_BDPT, "renderer::bdpt");

LM_NAMESPACE_END

