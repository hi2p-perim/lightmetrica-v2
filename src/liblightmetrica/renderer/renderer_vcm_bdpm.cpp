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
#include <lightmetrica/detail/parallel.h>
#include <lightmetrica/detail/photonmap.h>
#include <lightmetrica/detail/photonmaputils.h>

LM_NAMESPACE_BEGIN

struct PathVertex
{
    int type;
    SurfaceGeometry geom;
    const Primitive* primitive = nullptr;
};

using Subpath = std::vector<PathVertex>;
using Path = std::vector<PathVertex>;

struct KdTree
{
    struct Node
    {
        bool isleaf;
        Bound bound;

        union
        {
            struct
            {
                int begin;
                int end;
            } leaf;

            struct
            {
                int child1;
                int child2;
            } internal;
        };
    };

    struct Index
    {
        int subpathIndex;
        int vertexIndex;
    };

    std::vector<std::unique_ptr<Node>> nodes_;
    std::vector<int> indices_;
    std::vector<Index> vertices_;
    const std::vector<Subpath>& subpathLs_;

    KdTree(const std::vector<Subpath>& subpathLs)
        : subpathLs_(subpathLs)
    {
        // Arrange in a vector
        for (int i = 0; i < (int)subpathLs.size(); i++)
        {
            const auto& subpathL = subpathLs[i];
            for (int j = 1; j < (int)subpathL.size(); j++)
            {
                const auto& v = subpathL[j];
                if (!v.geom.infinite && !v.primitive->surface->IsDeltaPosition(v.type) && !v.primitive->surface->IsDeltaDirection(v.type))
                {
                    vertices_.push_back({ i, j });
                }
            }
        }

        // Build function
        const std::function<int(int, int)> Build_ = [&](int begin, int end) -> int
        {
            int idx = (int)(nodes_.size());
            nodes_.emplace_back(new Node);
            auto* node = nodes_[idx].get();

            // Current bound
            node->bound = Bound();
            for (int i = begin; i < end; i++)
            {
                const auto& v = vertices_[indices_[i]];
                node->bound = Math::Union(node->bound, subpathLs[v.subpathIndex][v.vertexIndex].geom.p);
            }

            // Create leaf node
            const int LeafNumNodes = 10;
            if (end - begin < LeafNumNodes)
            {
                node->isleaf = true;
                node->leaf.begin = begin;
                node->leaf.end = end;
                return idx;
            }

            // Select longest axis as split axis
            const int axis = node->bound.LongestAxis();

            // Select split position
            const Float split = node->bound.Centroid()[axis];

            // Partition into two sets according to split position
            const auto it = std::partition(indices_.begin() + begin, indices_.begin() + end, [&](int i) -> bool
            {
                const auto& v = vertices_[i];
                return subpathLs[v.subpathIndex][v.vertexIndex].geom.p[axis] < split;
            });

            // Create intermediate node
            const int mid = (int)(std::distance(indices_.begin(), it));
            node->isleaf = false;
            node->internal.child1 = Build_(begin, mid);
            node->internal.child2 = Build_(mid, end);

            return idx;
        };

        nodes_.clear();
        indices_.assign(vertices_.size(), 0);
        std::iota(indices_.begin(), indices_.end(), 0);
        Build_(0, (int)(vertices_.size()));
    }

    auto RangeQuery(const Vec3& p, Float radius, const std::function<void(int subpathIndex, int vertexIndex)>& queryFunc) const -> void
    {
        const Float radius2 = radius * radius;
        const std::function<void(int)> Collect = [&](int idx) -> void
        {
            const auto* node = nodes_.at(idx).get();

            if (node->isleaf)
            {
                for (int i = node->leaf.begin; i < node->leaf.end; i++)
                {
                    const auto& v = vertices_[indices_[i]];
                    if (Math::Length2(subpathLs_[v.subpathIndex][v.vertexIndex].geom.p - p) < radius2)
                    {
                        queryFunc(v.subpathIndex, v.vertexIndex);
                    }
                }
                return;
            }

            const int axis = node->bound.LongestAxis();
            const Float split = node->bound.Centroid()[axis];
            const auto dist2 = (p[axis] - split) * (p[axis] - split);
            if (p[axis] < split)
            {
                Collect(node->internal.child1);
                if (dist2 < radius2)
                {
                    Collect(node->internal.child2);
                }
            }
            else
            {
                Collect(node->internal.child2);
                if (dist2 < radius2)
                {
                    Collect(node->internal.child1);
                }
            }
        };

        Collect(0);
    }
};

// --------------------------------------------------------------------------------

///! Impelments BDPM as an intermediate implementation of VCM
class Renderer_VCM_BDPM final : public Renderer
{
public:

    LM_IMPL_CLASS(Renderer_VCM_BDPM, Renderer);

private:

    int maxNumVertices_;
    int minNumVertices_;
    long long numPhotonTraceSamples_;
    Scheduler::UniquePtr sched_ = ComponentFactory::Create<Scheduler>();

public:

    LM_IMPL_F(Initialize) = [this](const PropertyNode* prop) -> bool
    {
        sched_->Load(prop);
        maxNumVertices_        = prop->Child("max_num_vertices")->As<int>();
        minNumVertices_        = prop->Child("min_num_vertices")->As<int>();
        numPhotonTraceSamples_ = prop->ChildAs<long long>("num_photon_trace_samples", 100L);
        return true;
    };

    LM_IMPL_F(Render) = [this](const Scene* scene, Random* initRng, Film* film) -> void
    {
        #pragma region Helper functions

        const auto MergeRadius = 0.05_f;

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
            assert(s + t >= minNumVertices_);
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

        const auto MergeSubpaths = [&](Path& path, const Subpath& subpathL, const Subpath& subpathE, int s, int t) -> bool
        {
            assert(s >= 1);
            assert(t >= 1);
            assert(s + t >= minNumVertices_);
            assert(s + t <= maxNumVertices_);
            path.clear();
            const auto& vL = subpathL[s - 1];
            const auto& vE = subpathE[t - 1];
            if (vL.primitive->surface->IsDeltaPosition(vL.type) || vE.primitive->surface->IsDeltaPosition(vE.type)) { return false; }
            if (vL.geom.infinite || vE.geom.infinite) { return false; }
            path.insert(path.end(), subpathL.begin(), subpathL.begin() + s);
            path.insert(path.end(), subpathE.rend() - t, subpathE.rend());
            return true;
        };

        const auto EvaluateF = [&](Path& path, int s, bool merge) -> SPD
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
                for (int i = 0; i < (merge ? s : s - 1); i++)
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
            if (!merge)
            {
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
            }
            else
            {
                assert(s >= 1);
                assert(t >= 1);
                const auto& v     = path[s];
                const auto& vPrev = path[s-1];
                const auto& vNext = path[s+1];
                const auto fs = v.primitive->surface->EvaluateDirection(v.geom, v.type, Math::Normalize(vPrev.geom.p - v.geom.p), Math::Normalize(vNext.geom.p - v.geom.p), TransportDirection::LE, false);
                cst = fs / (Math::Pi() * MergeRadius * MergeRadius);
            }

            // --------------------------------------------------------------------------------

            return fL * cst * fE;
        };

        const auto EvaluatePathPDF = [&](const Path& path, int s, bool merge) -> PDFVal
        {
            const int n = (int)(path.size());
            const int t = n - s;
            assert(n >= 2);
            assert(n <= maxNumVertices_);

            if (!merge)
            {
                // Check if the path is samplable by vertex connection
                if (s == 0 && t > 0)
                {
                    const auto& v = path[0];
                    if (v.primitive->emitter->IsDeltaPosition(v.type)) { return PDFVal(PDFMeasure::ProdArea, 0_f); }
                }
                else if (s > 0 && t == 0)
                {
                    const auto& v = path[n - 1];
                    if (v.primitive->emitter->IsDeltaPosition(v.type)) { return PDFVal(PDFMeasure::ProdArea, 0_f); }
                }
                else if (s > 0 && t > 0)
                {
                    const auto& vL = path[s - 1];
                    const auto& vE = path[s];
                    if (vL.primitive->surface->IsDeltaDirection(vL.type) || vE.primitive->surface->IsDeltaDirection(vE.type)) { return PDFVal(PDFMeasure::ProdArea, 0_f); }
                }
            }
            else
            {
                // Check if the path is samplable by vertex merging
                if (s == 0 || t == 0) { return PDFVal(PDFMeasure::ProdArea, 0_f); }
                const auto& vE = path[s];
                if (vE.primitive->surface->IsDeltaPosition(vE.type) || vE.primitive->surface->IsDeltaDirection(vE.type)) { return PDFVal(PDFMeasure::ProdArea, 0_f); }
            }

            // Otherwise the path can be generated with the given strategy (s,t,merge) so p_{s,t,merge} can be safely evaluated.
            PDFVal pdf(PDFMeasure::ProdArea, 1_f);
            if (s > 0)
            {
                pdf *= path[0].primitive->emitter->EvaluatePositionGivenDirectionPDF(path[0].geom, Math::Normalize(path[1].geom.p - path[0].geom.p), false) * scene->EvaluateEmitterPDF(path[0].primitive).v;
                for (int i = 0; i < (merge ? s : s - 1); i++)
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

            //if (merge)
            //{
            //    pdf.v /= Math::Pi() * MergeRadius * MergeRadius;
            //}

            return pdf;
        };

        const auto EvaluateMISWeight = [&](const Path& path, int s_)
        {
            const int n = static_cast<int>(path.size());
            const auto ps = EvaluatePathPDF(path, s_, true);
            assert(ps > 0_f);

            Float invw = 0_f;
            for (int s = 0; s <= n; s++)
            {
                const auto pi = EvaluatePathPDF(path, s, true);
                if (pi > 0_f)
                {
                    const auto r = (pi.v * numPhotonTraceSamples_) / ps.v;
                    invw += r;
                }
            }

            return 1_f / invw;



            //const int n = (int)(path.size());
            //long long nonzero = 0;

            //for (int s = 0; s <= n; s++)
            //{
            //    const auto t = n - s;
            //    if (EvaluatePathPDF(path, s, true).v > 0_f)
            //    {
            //        nonzero++;
            //    }
            //}

            //assert(nonzero != 0);
            //return 1_f / (numPhotonTraceSamples_ * nonzero);

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

        // Sample light subpaths
        std::vector<Subpath> subpathLs;
        {
            LM_LOG_INFO("Sampling light subpaths");
            LM_LOG_INDENTER();

            struct Context
            {
                Random rng;
                std::vector<Subpath> subpathLs;
            };
            std::vector<Context> contexts(Parallel::GetNumThreads());
            for (auto& ctx : contexts) { ctx.rng.SetSeed(initRng->NextUInt()); }

            Parallel::For(numPhotonTraceSamples_, [&](long long index, int threadid, bool init)
            {
                auto& ctx = contexts[threadid];
                ctx.subpathLs.emplace_back();
                SampleSubpath(ctx.subpathLs.back(), &ctx.rng, TransportDirection::LE);
            });

            for (auto& ctx : contexts)
            {
                subpathLs.insert(subpathLs.end(), ctx.subpathLs.begin(), ctx.subpathLs.end());
            }
        }

        // Construct range query structure for vertices in light subpaths
        LM_LOG_INFO("Constructing range query structure");
        KdTree pm(subpathLs);

        // --------------------------------------------------------------------------------

        LM_LOG_INFO("Rendering");
        sched_->Process(scene, film, initRng, [&](Film* film, Random* rng) -> void
        {
            // Sample eye subpath
            Subpath subpathE;
            SampleSubpath(subpathE, rng, TransportDirection::EL);
            
            // --------------------------------------------------------------------------------

            // Combine subpaths
            const int nE = (int)(subpathE.size());
            for (int t = 1; t <= nE; t++)
            {
                const auto& vE = subpathE[t-1];
                if (vE.primitive->surface->IsDeltaPosition(vE.type))
                {
                    continue;
                }
                pm.RangeQuery(vE.geom.p, MergeRadius, [&](int si, int vi) -> void
                {
                    const int s = vi + 1;
                    const int n = s + t - 1;
                    if (n < minNumVertices_ || maxNumVertices_ < n) { return; }

                    // Merge vertices and create a full path
                    Path fullpath;
                    if (!MergeSubpaths(fullpath, subpathLs[si], subpathE, s - 1, t)) { return; }
                    
                    // Evaluate contribution
                    const auto f = EvaluateF(fullpath, s - 1, true);
                    if (f.Black()) { return; }

                    // Evaluate path PDF
                    const auto p = EvaluatePathPDF(fullpath, s - 1, true);

                    // Evaluate MIS weight
                    const auto w = EvaluateMISWeight(fullpath, s - 1);

                    // Accumulate contribution
                    const auto C = f * w / p;
                    film->Splat(RasterPosition(fullpath), C);
                });
            }
        });
    };

};

LM_COMPONENT_REGISTER_IMPL(Renderer_VCM_BDPM, "renderer::vcmbdpm");

LM_NAMESPACE_END
