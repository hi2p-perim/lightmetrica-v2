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
#include <lightmetrica/detail/photonmap.h>
#include <lightmetrica/detail/photonmaputils.h>

#define LM_VCMREF_DEBUG 0

LM_NAMESPACE_BEGIN

/*!
    \brief Vertex connection and merging renderer (reference version).

    Implements vertex conneection and merging [Georgiev et al. 2012].
    This implementation purposely adopts a naive way
    to check the correctness of the implementation and
    to be utilized as a baseline for the further modifications.

    For the optimized implementation, see `renderer::vcm`,
    which is based on the way described in the technical report [Georgiev 2012]
    or SmallVCM renderer [Davidovic & Georgiev 2012].

    References:
      - [Georgiev et al. 2012] Light transport simulation with vertex connection and merging
      - [Hachisuka et al. 2012] A path space extension for robust light transport simulation
      - [Georgiev 2012] Implementing vertex connection and merging
      - [Davidovic & Georgiev 2012] SmallVCM renderer 
*/
class Renderer_VCM_Reference final : public Renderer
{
public:

    LM_IMPL_CLASS(Renderer_VCM_Reference, Renderer);

private:

    int maxNumVertices_;
    int minNumVertices_;
    Scheduler::UniquePtr sched_ = ComponentFactory::Create<Scheduler>();

public:

    LM_IMPL_F(Initialize) = [this](const PropertyNode* prop) -> bool
    {
        sched_->Load(prop);
        maxNumVertices_ = prop->Child("max_num_vertices")->As<int>();
        minNumVertices_ = prop->Child("min_num_vertices")->As<int>();
        return true;
    };

    LM_IMPL_F(Render) = [this](const Scene* scene, Random* initRng, Film* film) -> void
    {
        #pragma region Helper functions

        struct PathVertex
        {
            int type;
            SurfaceGeometry geom;
            const Primitive* primitive = nullptr;
        };

        using Subpath = std::vector<PathVertex>;
        using Path = std::vector<PathVertex>;

        const auto SampleSubpath = [&](Subpath& subpath, Random* rng, TransportDirection transDir) -> void
        {
            PhotonMapUtils::TraceSubpath(scene, rng, maxNumVertices_, transDir, [&](int numVertices, const Vec2& /*rasterPos*/, const PhotonMapUtils::PathVertex& pv, const PhotonMapUtils::PathVertex& v, SPD& throughput) -> bool
            {
                PathVertex v_;
                v_.type = v.type;
                v_.geom = v.geom;
                v_.primitive = v.primitive;
                subpath.emplace_back(v_);
                return true;
            });
        };

        const auto ConnectSubpaths = [&](Path& path, const Subpath& subpathL, const Subpath& subpathE, int s, int t) -> bool
        {
            assert(s >= 0);
            assert(t >= 0);
            assert(s + t >= 2);
            assert(s + t <= maxNumVertices_);
            path.clear();
            if (s == 0 && t > 0)
            {
                path.insert(path.end(), subpathE.rbegin(), subpathE.rend());
                if ((path.front().primitive->surface->Type() & SurfaceInteractionType::L) == 0) { return false; }
                path.front().type = SurfaceInteractionType::L;
            }
            else if (s > 0 && t == 0)
            {
                path.insert(path.end(), subpathL.begin(), subpathL.end());
                if ((path.back().primitive->surface->Type() & SurfaceInteractionType::E) == 0) { return false; }
                path.back().type = SurfaceInteractionType::E;
            }
            else
            {
                const auto& vL = subpathL[s - 1];
                const auto& vE = subpathE[t - 1];
                if (vL.geom.infinite || vE.geom.infinite)  { return false; }
                if (!scene->Visible(vL.geom.p, vE.geom.p)) { return false; }
                path.insert(path.end(), subpathL.begin(), subpathL.begin() + s);
                path.insert(path.end(), subpathE.rend() - t, subpathE.rend());
            }
            return true;
        };

        const auto EvaluateF = [&](Path& path, int s) -> SPD
        {
            const int n = (int)(path.size());
            const int t = n - s;
            assert(n >= 2);
            assert(n <= maxNumVertices_);

            // --------------------------------------------------------------------------------

            SPD fL;
            if (s == 0) { fL = SPD(1_f); }
            else
            {
                {
                    const auto* vL = &path[0];
                    fL = vL->primitive->emitter->EvaluatePosition(vL->geom, false);
                }
                for (int i = 0; i < s - 1; i++)
                {
                    const auto* v = &path[i];
                    const auto* vPrev = i >= 1 ? &path[i - 1] : nullptr;
                    const auto* vNext = &path[i + 1];
                    const auto wi = vPrev ? Math::Normalize(vPrev->geom.p - v->geom.p) : Vec3();
                    const auto wo = Math::Normalize(vNext->geom.p - v->geom.p);
                    fL *= v->primitive->surface->EvaluateDirection(v->geom, v->type, wi, wo, TransportDirection::LE, false);
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
                    const auto* vE = &path[n - 1];
                    fE = vE->primitive->emitter->EvaluatePosition(vE->geom, false);
                }
                for (int i = n - 1; i > s; i--)
                {
                    const auto* v = &path[i];
                    const auto* vPrev = &path[i - 1];
                    const auto* vNext = i < n - 1 ? &path[i + 1] : nullptr;
                    const auto wi = vNext ? Math::Normalize(vNext->geom.p - v->geom.p) : Vec3();
                    const auto wo = Math::Normalize(vPrev->geom.p - v->geom.p);
                    fE *= v->primitive->surface->EvaluateDirection(v->geom, v->type, wi, wo, TransportDirection::EL, false);
                    fE *= RenderUtils::GeometryTerm(v->geom, vPrev->geom);
                }
            }
            if (fE.Black()) { return SPD(); }

            // --------------------------------------------------------------------------------

            SPD cst;
            if (s == 0 && t > 0)
            {
                const auto& v = path[0];
                const auto& vNext = path[1];
                cst = v.primitive->emitter->EvaluatePosition(v.geom, true) * v.primitive->emitter->EvaluateDirection(v.geom, v.type, Vec3(), Math::Normalize(vNext.geom.p - v.geom.p), TransportDirection::EL, false);
            }
            else if (s > 0 && t == 0)
            {
                const auto& v = path[n - 1];
                const auto& vPrev = path[n - 2];
                cst = v.primitive->emitter->EvaluatePosition(v.geom, true) * v.primitive->emitter->EvaluateDirection(v.geom, v.type, Vec3(), Math::Normalize(vPrev.geom.p - v.geom.p), TransportDirection::LE, false);
            }
            else if (s > 0 && t > 0)
            {
                const auto* vL = &path[s - 1];
                const auto* vE = &path[s];
                const auto* vLPrev = s - 2 >= 0 ? &path[s - 2] : nullptr;
                const auto* vENext = s + 1 < n ? &path[s + 1] : nullptr;
                const auto fsL = vL->primitive->surface->EvaluateDirection(vL->geom, vL->type, vLPrev ? Math::Normalize(vLPrev->geom.p - vL->geom.p) : Vec3(), Math::Normalize(vE->geom.p - vL->geom.p), TransportDirection::LE, true);
                const auto fsE = vE->primitive->surface->EvaluateDirection(vE->geom, vE->type, vENext ? Math::Normalize(vENext->geom.p - vE->geom.p) : Vec3(), Math::Normalize(vL->geom.p - vE->geom.p), TransportDirection::EL, true);
                const Float G = RenderUtils::GeometryTerm(vL->geom, vE->geom);
                cst = fsL * G * fsE;
            }

            // --------------------------------------------------------------------------------

            return fL * cst * fE;
        };

        const auto EvaluateConnectionPDF = [&](const Path& path, int s) -> PDFVal
        {
            const int n = (int)(path.size());
            const int t = n - s;
            assert(n >= 2);
            assert(n <= maxNumVertices_);

            // Check if the path is samplable by vertex connection
            if (s == 0 && t > 0)
            {
                if (path[0].primitive->emitter->IsDeltaPosition()) { return PDFVal(PDFMeasure::ProdArea, 0_f); }
            }
            else if (s > 0 && t == 0)
            {
                if (path[n - 1].primitive->emitter->IsDeltaPosition()) { return PDFVal(PDFMeasure::ProdArea, 0_f); }
            }
            else if (s > 0 && t > 0)
            {
                const auto& vL = path[s - 1];
                const auto& vE = path[s];
                if (vL.primitive->surface->IsDeltaDirection(vL.type) || vE.primitive->surface->IsDeltaDirection(vE.type)) { return PDFVal(PDFMeasure::ProdArea, 0_f); }
            }

            // Otherwise the path can be generated with the given strategy (s,t)
            // so p_{s,t} can be safely evaluated.
            PDFVal pdf(PDFMeasure::ProdArea, 1_f);
            if (s > 0)
            {
                pdf *= path[0].primitive->emitter->EvaluatePositionGivenDirectionPDF(path[0].geom, Math::Normalize(path[1].geom.p - path[0].geom.p), false) * scene->EvaluateEmitterPDF(path[0].primitive).v;
                for (int i = 0; i < s - 1; i++)
                {
                    const auto* vi = &path[i];
                    const auto* vip = i - 1 >= 0 ? &path[i - 1] : nullptr;
                    const auto* vin = &path[i + 1];
                    pdf *= vi->primitive->surface->EvaluateDirectionPDF(vi->geom, vi->type, vip ? Math::Normalize(vip->geom.p - vi->geom.p) : Vec3(), Math::Normalize(vin->geom.p - vi->geom.p), false).ConvertToArea(vi->geom, vin->geom);
                }
            }
            if (t > 0)
            {
                pdf *= path[n - 1].primitive->emitter->EvaluatePositionGivenDirectionPDF(path[n - 1].geom, Math::Normalize(path[n - 2].geom.p - path[n - 1].geom.p), false) * scene->EvaluateEmitterPDF(path[n - 1].primitive).v;
                for (int i = n - 1; i >= s + 1; i--)
                {
                    const auto* vi = &path[i];
                    const auto* vip = &path[i - 1];
                    const auto* vin = i + 1 < n ? &path[i + 1] : nullptr;
                    pdf *= vi->primitive->surface->EvaluateDirectionPDF(vi->geom, vi->type, vin ? Math::Normalize(vin->geom.p - vi->geom.p) : Vec3(), Math::Normalize(vip->geom.p - vi->geom.p), false).ConvertToArea(vi->geom, vip->geom);
                }
            }

            return pdf;
        };

        const auto EvaluateMISWeight = [&](const Path& path, int s_)
        {
            //const int n = static_cast<int>(path.size());
            //const auto ps = EvaluateConnectionPDF(path, s_);
            //assert(ps > 0_f);

            //Float invw = 0_f;
            //for (int s = 0; s <= n; s++)
            //{
            //    const auto t = n - s;
            //    const auto pi = EvaluateConnectionPDF(path, s);
            //    if (pi > 0_f)
            //    {
            //        const auto r = pi.v / ps.v;
            //        invw += r * r;
            //    }
            //}

            //return 1_f / invw;



            const int n = (int)(path.size());
            int nonzero = 0;

            for (int s = 0; s <= n; s++)
            {
                const auto t = n - s;
                if (EvaluateConnectionPDF(path, s).v > 0_f)
                {
                    nonzero++;
                }
            }

            assert(nonzero != 0);
            return 1_f / nonzero;

        };

        const auto RasterPosition = [&](const Path& path) -> Vec2
        {
            const auto& v = path[path.size() - 1];
            const auto& vPrev = path[path.size() - 2];
            Vec2 rasterPos;
            v.primitive->sensor->RasterPosition(Math::Normalize(vPrev.geom.p - v.geom.p), v.geom, rasterPos);
            return rasterPos;
        };

        #pragma endregion

        // --------------------------------------------------------------------------------

        #if LM_VCMREF_DEBUG
        struct Strategy
        {
            int s;
            int t;
            bool operator==(const Strategy& o) const
            {
                return (s == o.s && t == o.t);
            }
        };
        struct StrategyHash
        {
            const int N = 100;
            auto operator()(const Strategy& v) const -> size_t
            {
                return v.s * N + v.t;
            }
        };
        std::vector<Film::UniquePtr> strategyFilms1;
        std::vector<Film::UniquePtr> strategyFilms2;
        std::unordered_map<Strategy, size_t, StrategyHash> strategyFilmMap;
        std::mutex strategyFilmMutex;
        #endif

        // --------------------------------------------------------------------------------

        const auto processedSamples = sched_->Process(scene, film, initRng, [&](Film* film, Random* rng) -> void
        {
            // Sample subpaths
            Subpath subpathL;
            Subpath subpathE;
            SampleSubpath(subpathL, rng, TransportDirection::LE);
            SampleSubpath(subpathE, rng, TransportDirection::EL);
            
            // Combine subpaths
            const int nL = (int)(subpathL.size());
            const int nE = (int)(subpathE.size());
            for (int t = 0; t <= nE; t++)
            {
                const int minS = Math::Max(0, Math::Max(2-t, minNumVertices_-t));
                const int maxS = Math::Min(nL, maxNumVertices_-t);
                for (int s = minS; s <= maxS; s++)
                {
                    #pragma region Vertex conection

                    // Connect vertices and create a full path
                    Path fullpath;
                    if (!ConnectSubpaths(fullpath, subpathL, subpathE, s, t)) { continue; }

                    // Evaluate contribution
                    const auto f = EvaluateF(fullpath, s);
                    if (f.Black()) { continue; }
                    
                    // Evaluate connection PDF
                    const auto p = EvaluateConnectionPDF(fullpath, s);

                    // Evaluate MIS weight
                    const auto w = EvaluateMISWeight(fullpath, s);

                    // Accumulate contribution
                    const auto C = f * w / p;
                    film->Splat(RasterPosition(fullpath), C);

                    #if LM_VCMREF_DEBUG
                    {
                        const auto Cstar = f / p;
                        std::unique_lock<std::mutex> lock(strategyFilmMutex);
                        Strategy strategy{ s, t };
                        if (strategyFilmMap.find(strategy) == strategyFilmMap.end())
                        {
                            strategyFilms1.push_back(ComponentFactory::Clone<Film>(film));
                            strategyFilms2.push_back(ComponentFactory::Clone<Film>(film));
                            strategyFilms1.back()->Clear();
                            strategyFilms2.back()->Clear();
                            strategyFilmMap[strategy] = strategyFilms1.size() - 1;
                        }
                        strategyFilms1[strategyFilmMap[strategy]]->Splat(RasterPosition(fullpath), C);
                        strategyFilms2[strategyFilmMap[strategy]]->Splat(RasterPosition(fullpath), Cstar);
                    }
                    #endif

                    #pragma endregion
                }
            }
        });

        // --------------------------------------------------------------------------------

        #if LM_VCMREF_DEBUG
        for (const auto& kv : strategyFilmMap)
        {
            const auto* f1 = strategyFilms1[kv.second].get();
            const auto* f2 = strategyFilms2[kv.second].get();
            f1->Rescale((Float)(f1->Width() * f1->Height()) / processedSamples);
            f2->Rescale((Float)(f2->Width() * f2->Height()) / processedSamples);
            f1->Save(boost::str(boost::format("vcmref_f1_n%02d_s%02d_t%02d") % (kv.first.s + kv.first.t) % kv.first.s % kv.first.t));
            f2->Save(boost::str(boost::format("vcmref_f2_n%02d_s%02d_t%02d") % (kv.first.s + kv.first.t) % kv.first.s % kv.first.t));
        }
        #else
        LM_UNUSED(processedSamples);
        #endif
    };

};

LM_COMPONENT_REGISTER_IMPL(Renderer_VCM_Reference, "renderer::vcmref");

LM_NAMESPACE_END
