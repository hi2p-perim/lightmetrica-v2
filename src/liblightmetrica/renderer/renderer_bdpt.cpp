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
#include <lightmetrica/surfacegeometry.h>
#include <lightmetrica/primitive.h>
#include <lightmetrica/scheduler.h>
#include <lightmetrica/renderutils.h>

LM_NAMESPACE_BEGIN

struct PathVertex
{
    int type = SurfaceInteraction::None;
    SurfaceGeometry geom;
    const Primitive* primitive = nullptr;
};

struct Path
{

    std::vector<PathVertex> vertices;

public:

    #pragma region BDPT path initialization

    auto SampleSubpath(const Scene* scene, Random* rng, TransportDirection transDir, int maxPathVertices) -> void
    {
        PathVertex v;
        vertices.clear();
        for (int step = 0; maxPathVertices == -1 || step < maxPathVertices; step++)
        {
            if (step == 0)
            {
                #pragma region Sample initial vertex

                // Sample an emitter
                const auto type = transDir == TransportDirection::LE ? SurfaceInteraction::L : SurfaceInteraction::E;
                const auto* emitter = scene->SampleEmitter(type, rng->Next());
                v.primitive = emitter;
                v.type = type;

                // Sample a position on the emitter
                emitter->SamplePosition(rng->Next2D(), v.geom);

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
                pv->primitive->SampleDirection(rng->Next2D(), rng->Next(), pv->type, pv->geom, wi, wo);
                const auto f = pv->primitive->EvaluateDirection(pv->geom, pv->type, wi, wo, transDir, false);
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
                v.geom = isect.geom;
                v.primitive = isect.primitive;
                v.type = isect.primitive->Type() & ~SurfaceInteraction::Emitter;

                // Path termination
                const Float rrProb = 0.5_f;
                if (rng->Next() > rrProb)
                {
                    vertices.push_back(v);
                    break;
                }

                // Add a vertex
                vertices.push_back(v);

                #pragma endregion
            }
        }
    }

    auto Connect(const Scene* scene, int s, int t, const Path& subpathL, const Path& subpathE) -> bool
    {
        assert(s > 0 || t > 0);

        vertices.clear();

        if (s == 0 && t > 0)
        {
            if ((subpathE.vertices[t - 1].primitive->Type() & SurfaceInteraction::L) == 0)
            {
                return false;
            }
            for (int i = t - 1; i >= 0; i--)
            {
                vertices.push_back(subpathE.vertices[i]);
            }
            vertices.front().type = SurfaceInteraction::L;
        }
        else if (s > 0 && t == 0)
        {
            if ((subpathL.vertices[s - 1].primitive->Type() & SurfaceInteraction::E) == 0)
            {
                return false;
            }
            for (int i = 0; i < s; i++)
            {
                vertices.push_back(subpathL.vertices[i]);
            }
            vertices.back().type = SurfaceInteraction::E;
        }
        else
        {
            assert(s > 0 && t > 0);
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

    auto EvaluateContribution(const Scene* scene, int s) const -> SPD
    {
        const auto Cstar = EvaluateUnweightContribution(scene, s);
        return Cstar.Black() ? SPD() : Cstar * EvaluatePowerHeuristicsMISWeightOpt(scene, s);
    }

    auto SelectionProb(int s) const -> Float
    {
        const Float rrProb = 0.5_f;
        const int n = (int)(vertices.size());
        const int t = n - s;
        Float selectionProb = 1;
        for (int i = 1; i < s - 1; i++)
        {
            selectionProb *= rrProb;
        }
        for (int i = t - 2; i >= 1; i--)
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
        v.primitive->emitter->RasterPosition(Math::Normalize(vPrev.geom.p - v.geom.p), v.geom, rasterPos);
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
            cst = v.primitive->EvaluatePosition(v.geom, true) * v.primitive->EvaluateDirection(v.geom, v.type, Vec3(), Math::Normalize(vNext.geom.p - v.geom.p), TransportDirection::EL, true);
        }
        else if (s > 0 && t == 0)
        {
            const auto& v = vertices[n - 1];
            const auto& vPrev = vertices[n - 2];
            cst = v.primitive->EvaluatePosition(v.geom, true) * v.primitive->EvaluateDirection(v.geom, v.type, Vec3(), Math::Normalize(vPrev.geom.p - v.geom.p), TransportDirection::LE, true);
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

    auto EvaluateUnweightContribution(const Scene* scene, int s) const -> SPD
    {
        const int n = (int)(vertices.size());
        const int t = n - s;

        // --------------------------------------------------------------------------------

        #pragma region Function to compute local contribution

        const auto LocalContrb = [](const SPD& f, Float p) -> SPD
        {
            assert(p != 0 || (p == 0 && f.Black()));
            if (f.Black()) return SPD();
            return f / p;
        };

        #pragma endregion

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
                const auto& v = vertices[0];
                alphaL = LocalContrb(v.primitive->EvaluatePosition(v.geom, false), v.primitive->EvaluatePositionPDF(v.geom, false) * scene->EvaluateEmitterPDF(v.primitive));
            }
            for (int i = 0; i < s - 1; i++)
            {
                const auto* v     = &vertices[i];
                const auto* vPrev = i >= 1 ? &vertices[i - 1] : nullptr;
                const auto* vNext = &vertices[i + 1];
                const auto wi = vPrev ? Math::Normalize(vPrev->geom.p - v->geom.p) : Vec3();
                const auto wo = Math::Normalize(vNext->geom.p - v->geom.p);
                alphaL *= LocalContrb(v->primitive->EvaluateDirection(v->geom, v->type, wi, wo, TransportDirection::LE, false), v->primitive->EvaluateDirectionPDF(v->geom, v->type, wi, wo, false));
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
                const auto& v = vertices[n - 1];
                alphaE = LocalContrb(v.primitive->EvaluatePosition(v.geom, false), v.primitive->EvaluatePositionPDF(v.geom, false) * scene->EvaluateEmitterPDF(v.primitive));
            }
            for (int i = n - 1; i > s; i--)
            {
                const auto* v = &vertices[i];
                const auto* vPrev = &vertices[i - 1];
                const auto* vNext = i < n - 1 ? &vertices[i + 1] : nullptr;
                const auto wi = vNext ? Math::Normalize(vNext->geom.p - v->geom.p) : Vec3();
                const auto wo = Math::Normalize(vPrev->geom.p - v->geom.p);
                alphaE *= LocalContrb(v->primitive->EvaluateDirection(v->geom, v->type, wi, wo, TransportDirection::EL, false), v->primitive->EvaluateDirectionPDF(v->geom, v->type, wi, wo, false));
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

    auto EvaluateSimpleMISWeight(const Scene* scene, int s) const -> Float
    {
        const int n = (int)(vertices.size());
        int nonzero = 0;

        for (int i = 0; i <= n; i++)
        {
            if (EvaluatePDF(scene, i) > 0)
            {
                nonzero++;
            }
        }

        assert(nonzero != 0);
        return 1_f / nonzero;
    }

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

    auto EvaluatePDF(const Scene* scene, int s) const -> Float
    {
        // Cases with p_{s,t}(x) = 0
        // i.e. the strategy (s,t) cannot generate the path
        // This condition is equivalent to c_{s,t}(x) = 0
        if (EvaluateCst(s).Black())
        {
            return 0;
        }

        // Otherwise the path can be generated with the given strategy (s,t)
        // so p_{s,t} can be safely evaluated.
        Float pdf = 1;
        const int n = (int)(vertices.size());
        const int t = n - s;
        if (s > 0)
        {
            pdf *= vertices[0].primitive->EvaluatePositionPDF(vertices[0].geom, false) * scene->EvaluateEmitterPDF(vertices[0].primitive);
            for (int i = 0; i < s - 1; i++)
            {
                const auto* vi = &vertices[i];
                const auto* vip = i - 1 >= 0 ? &vertices[i - 1] : nullptr;
                const auto* vin = &vertices[i + 1];
                pdf *= vi->primitive->EvaluateDirectionPDF(vi->geom, vi->type, vip ? Math::Normalize(vip->geom.p - vi->geom.p) : Vec3(), Math::Normalize(vin->geom.p - vi->geom.p), false);
                pdf *= RenderUtils::GeometryTerm(vi->geom, vin->geom);
            }
        }
        if (t > 0)
        {
            pdf *= vertices[n - 1].primitive->EvaluatePositionPDF(vertices[n - 1].geom, false) * scene->EvaluateEmitterPDF(vertices[n - 1].primitive);
            for (int i = n - 1; i >= s + 1; i--)
            {
                const auto* vi = &vertices[i];
                const auto* vip = &vertices[i - 1];
                const auto* vin = i + 1 < n ? &vertices[i + 1] : nullptr;
                pdf *= vi->primitive->EvaluateDirectionPDF(vi->geom, vi->type, vin ? Math::Normalize(vin->geom.p - vi->geom.p) : Vec3(), Math::Normalize(vip->geom.p - vi->geom.p), false);
                pdf *= RenderUtils::GeometryTerm(vi->geom, vip->geom);
            }
        }

        return pdf;
    }

    #pragma endregion

};

// --------------------------------------------------------------------------------

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

        static thread_local Path subpathL, subpathE;
        static thread_local Path path;

        sched_->Process(scene, film, &initRng, [&](const Scene* scene, Film* film, Random* rng)
        {
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

                    #pragma region Evaluate contribution

                    const auto C = path.EvaluateContribution(scene, s) / path.SelectionProb(s);
                    if (C.Black())
                    {
                        continue;
                    }

                    #pragma endregion

                    // --------------------------------------------------------------------------------

                    #pragma region Accumulate to film

                    film->Splat(path.RasterPosition(), C);

                    #pragma endregion
                }
            }

            #pragma endregion
        });
    };

private:

    int maxNumVertices_;
    Scheduler::UniquePtr sched_;

};

LM_COMPONENT_REGISTER_IMPL(Renderer_BDPT, "renderer::bdpt");

LM_NAMESPACE_END

