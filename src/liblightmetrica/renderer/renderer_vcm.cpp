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
#include <lightmetrica/renderutils.h>
#include <lightmetrica/primitive.h>
#include <lightmetrica/scene.h>
#include <lightmetrica/random.h>
#include <lightmetrica/sensor.h>
#include <lightmetrica/detail/parallel.h>
#include <lightmetrica/detail/photonmap.h>
#include <lightmetrica/detail/subpathsampler.h>

#define LM_VCM_DEBUG 1  

LM_NAMESPACE_BEGIN

struct VCMPathVertex
{
    int type;
    SurfaceGeometry geom;
    const Primitive* primitive = nullptr;
};

struct VCMSubpath
{
    std::vector<VCMPathVertex> vertices;
    auto SampleSubpath(const Scene* scene, Random* rng, TransportDirection transDir, int maxNumVertices) -> void
    {
        vertices.clear();
        SubpathSampler::TraceSubpath(scene, rng, maxNumVertices, transDir, [&](int numVertices, const Vec2& /*rasterPos*/, const SubpathSampler::PathVertex& pv, const SubpathSampler::PathVertex& v, SPD& throughput) -> bool
        {
            VCMPathVertex v_;
            v_.type = v.type;
            v_.geom = v.geom;
            v_.primitive = v.primitive;
            vertices.emplace_back(v_);
            return true;
        });
    }
};

struct VCMPath
{
    std::vector<VCMPathVertex> vertices;

    auto ConnectSubpaths(const Scene* scene, const VCMSubpath& subpathL, const VCMSubpath& subpathE, int s, int t) -> bool
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

    auto MergeSubpaths(const VCMSubpath& subpathL, const VCMSubpath& subpathE, int s, int t) -> bool
    {
        assert(s >= 1);
        assert(t >= 1);
        vertices.clear();
        const auto& vL = subpathL.vertices[s - 1];
        const auto& vE = subpathE.vertices[t - 1];
        if (vL.primitive->IsDeltaPosition(vL.type) || vE.primitive->IsDeltaPosition(vE.type)) { return false; }
        if (vL.geom.infinite || vE.geom.infinite) { return false; }
        vertices.insert(vertices.end(), subpathL.vertices.begin(), subpathL.vertices.begin() + s);
        vertices.insert(vertices.end(), subpathE.vertices.rend() - t, subpathE.vertices.rend());
        return true;
    }

    auto EvaluateF(int s, bool merge) const -> SPD
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
            for (int i = 0; i < (merge ? s : s - 1); i++)
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
        if (!merge)
        {
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
        }
        else
        {
            assert(s >= 1);
            assert(t >= 1);
            const auto& v = vertices[s];
            const auto& vPrev = vertices[s - 1];
            const auto& vNext = vertices[s + 1];
            const auto fs = v.primitive->EvaluateDirection(v.geom, v.type, Math::Normalize(vNext.geom.p - v.geom.p), Math::Normalize(vPrev.geom.p - v.geom.p), TransportDirection::EL, true);
            cst = fs;
        }

        // --------------------------------------------------------------------------------

        return fL * cst * fE;
    }

    auto EvaluatePathPDF(const Scene* scene, int s, bool merge, Float radius) const -> PDFVal
    {
        const int n = (int)(vertices.size());
        const int t = n - s;
        assert(n >= 2);

        if (!merge)
        {
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
        }
        else
        {
            // Check if the path is samplable by vertex merging
            if (s == 0 || t == 0) { return PDFVal(PDFMeasure::ProdArea, 0_f); }
            const auto& vE = vertices[s];
            if (vE.primitive->IsDeltaPosition(vE.type) || vE.primitive->IsDeltaDirection(vE.type)) { return PDFVal(PDFMeasure::ProdArea, 0_f); }
        }

        // Otherwise the path can be generated with the given strategy (s,t,merge) so p_{s,t,merge} can be safely evaluated.
        PDFVal pdf(PDFMeasure::ProdArea, 1_f);
        if (s > 0)
        {
            pdf *= vertices[0].primitive->EvaluatePositionGivenDirectionPDF(vertices[0].geom, Math::Normalize(vertices[1].geom.p - vertices[0].geom.p), false) * scene->EvaluateEmitterPDF(vertices[0].primitive).v;
            for (int i = 0; i < (merge ? s : s - 1); i++)
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

        if (merge)
        {
            pdf.v *= (Math::Pi() * radius * radius);
        }

        return pdf;
    }

    auto EvaluateMISWeight_VCM(const Scene* scene, int s_, bool merge, Float radius, long long numPhotonTraceSamples) const -> Float
    {
        const int n = static_cast<int>(vertices.size());
        const auto ps = EvaluatePathPDF(scene, s_, merge, radius);
        assert(ps > 0_f);

        Float invw = 0_f;
        for (int s = 0; s <= n; s++)
        {
            for (int type = 0; type < 2; type++)
            {
                const auto pi = EvaluatePathPDF(scene, s, type > 0, radius);
                if (pi > 0_f)
                {
                    const auto r = pi.v / ps.v;
                    invw += r*r*(type > 0 ? (Float)(numPhotonTraceSamples) : 1_f);
                }
            }
        }

        return 1_f / invw;
    }

    auto EvaluateMISWeight_BDPT(const Scene* scene, int s_) const -> Float
    {
        const int n = static_cast<int>(vertices.size());
        const auto ps = EvaluatePathPDF(scene, s_, false, 0_f);
        assert(ps > 0_f);

        Float invw = 0_f;
        for (int s = 0; s <= n; s++)
        {
            const auto pi = EvaluatePathPDF(scene, s, false, 0_f);
            if (pi > 0_f)
            {
                const auto r = pi.v / ps.v;
                invw += r*r;
            }
        }

        return 1_f / invw;
    }

    auto EvaluateMISWeight_BDPM(const Scene* scene, int s_, Float radius, long long numPhotonTraceSamples) const -> Float
    {
        const int n = static_cast<int>(vertices.size());
        const auto ps = EvaluatePathPDF(scene, s_, true, radius);
        assert(ps > 0_f);

        Float invw = 0_f;
        for (int s = 0; s <= n; s++)
        {
            const auto pi = EvaluatePathPDF(scene, s, true, radius);
            if (pi > 0_f)
            {
                const auto r = pi.v / ps.v;
                invw += r*r*(Float)(numPhotonTraceSamples);
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

struct VCMKdTree
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
    const std::vector<VCMSubpath>& subpathLs_;

    VCMKdTree(const std::vector<VCMSubpath>& subpathLs)
        : subpathLs_(subpathLs)
    {}

    auto Build() -> void
    {
        // Arrange in a vector
        for (int i = 0; i < (int)subpathLs_.size(); i++)
        {
            const auto& subpathL = subpathLs_[i];
            for (int j = 1; j < (int)subpathL.vertices.size(); j++)
            {
                const auto& v = subpathL.vertices[j];
                if (!v.geom.infinite && !v.primitive->IsDeltaPosition(v.type) && !v.primitive->IsDeltaDirection(v.type))
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
                node->bound = Math::Union(node->bound, subpathLs_[v.subpathIndex].vertices[v.vertexIndex].geom.p);
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
                return subpathLs_[v.subpathIndex].vertices[v.vertexIndex].geom.p[axis] < split;
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
                    if (Math::Length2(subpathLs_[v.subpathIndex].vertices[v.vertexIndex].geom.p - p) < radius2)
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

enum class Mode
{
    VCM,
    BDPT,
    BDPM,
};

/*!
    \brief Vertex connection and merging renderer (reference version).

    Implements vertex connection and merging [Georgiev et al. 2012].
    This implementation purposely adopts a naive way
    to check the correctness of the implementation and
    to be utilized as a baseline for the further modifications.

    For the optimized implementation, see `renderer::vcmopt` (TODO),
    which is based on the way described in the technical report [Georgiev 2012]
    or SmallVCM renderer [Davidovic & Georgiev 2012].

    References:
      - [Georgiev et al. 2012] Light transport simulation with vertex connection and merging
      - [Hachisuka et al. 2012] A path space extension for robust light transport simulation
      - [Georgiev 2012] Implementing vertex connection and merging
      - [Davidovic & Georgiev 2012] SmallVCM renderer 
*/
class Renderer_VCM final : public Renderer
{
public:

    LM_IMPL_CLASS(Renderer_VCM, Renderer);

private:

    int maxNumVertices_;
    int minNumVertices_;
    long long numIterationPass_;
    long long numPhotonTraceSamples_;
    long long numEyeTraceSamples_;
    Float initialRadius_;
    Float alpha_;
    Mode mode_;
    #if LM_VCM_DEBUG
    std::string debugOutputPath_;
    #endif

public:

    LM_IMPL_F(Initialize) = [this](const PropertyNode* p) -> bool
    {
        maxNumVertices_        = p->ChildAs<int>("max_num_vertices", 10);
        minNumVertices_        = p->ChildAs<int>("min_num_vertices", 0);
        numIterationPass_      = p->ChildAs<long long>("num_iteration_pass", 100L);
        numPhotonTraceSamples_ = p->ChildAs<long long>("num_photon_trace_samples", 10000L);
        numEyeTraceSamples_    = p->ChildAs<long long>("num_eye_trace_samples", 10000L);
        initialRadius_         = p->ChildAs<Float>("initial_radius", 0.1_f);
        alpha_                 = p->ChildAs<Float>("alpha", 0.7_f);
        #if LM_VCM_DEBUG
        debugOutputPath_       = p->ChildAs<std::string>("debug_output_path", "vcm_%05d");
        #endif
        {
            const auto modestr = p->ChildAs<std::string>("mode", "vcm");
            if (modestr == "vcm") { mode_ = Mode::VCM; }
            else if (modestr == "bdpt") { mode_ = Mode::BDPT; }
            else if (modestr == "bdpm") { mode_ = Mode::BDPM; }
            LM_LOG_INFO("Selected mode: '" + modestr + "'");
        }
        return true;
    };

    LM_IMPL_F(Render) = [this](const Scene* scene, Random* initRng, Film* film) -> void
    {
        Float mergeRadius = 0_f;
        for (long long pass = 0; pass < numIterationPass_; pass++)
        {
            LM_LOG_INFO("Pass " + std::to_string(pass));
            LM_LOG_INDENTER();

            // --------------------------------------------------------------------------------

            #pragma region Update merge radius
            mergeRadius = pass == 0 ? initialRadius_ : std::sqrt((alpha_ + pass) / (1_f + pass)) * mergeRadius;
            #pragma endregion

            // --------------------------------------------------------------------------------

            #pragma region Sample light subpaths
            std::vector<VCMSubpath> subpathLs;
            if (mode_ == Mode::VCM || mode_ == Mode::BDPM)
            {
                LM_LOG_INFO("Sampling light subpaths");
                LM_LOG_INDENTER();

                struct Context
                {
                    Random rng;
                    std::vector<VCMSubpath> subpathLs;
                };
                std::vector<Context> contexts(Parallel::GetNumThreads());
                for (auto& ctx : contexts) { ctx.rng.SetSeed(initRng->NextUInt()); }

                Parallel::For(numPhotonTraceSamples_, [&](long long index, int threadid, bool init)
                {
                    auto& ctx = contexts[threadid];
                    ctx.subpathLs.emplace_back();
                    ctx.subpathLs.back().SampleSubpath(scene, &ctx.rng, TransportDirection::LE, maxNumVertices_);
                });

                for (auto& ctx : contexts)
                {
                    subpathLs.insert(subpathLs.end(), ctx.subpathLs.begin(), ctx.subpathLs.end());
                }
            }
            #pragma endregion

            // --------------------------------------------------------------------------------

            #pragma region Construct range query structure for vertices in light subpaths
            VCMKdTree pm(subpathLs);
            if (mode_ == Mode::VCM || mode_ == Mode::BDPM)
            {
                LM_LOG_INFO("Constructing range query structure");
                LM_LOG_INDENTER();
                pm.Build();
            }
            #pragma endregion

            // --------------------------------------------------------------------------------

            #pragma region Estimate contribution
            {
                LM_LOG_INFO("Estimating contribution");
                LM_LOG_INDENTER();

                struct Context
                {
                    Random rng;
                    Film::UniquePtr film{nullptr, nullptr};
                };
                std::vector<Context> contexts(Parallel::GetNumThreads());
                for (auto& ctx : contexts)
                {
                    ctx.rng.SetSeed(initRng->NextUInt());
                    ctx.film = ComponentFactory::Clone<Film>(film);
                    ctx.film->Clear();
                }

                Parallel::For(numEyeTraceSamples_, [&](long long index, int threadid, bool init)
                {
                    auto& ctx = contexts[threadid];

                    // --------------------------------------------------------------------------------

                    #pragma region Sample subpaths
                    static thread_local VCMSubpath subpathE;
                    static thread_local VCMSubpath subpathL;
                    subpathE.SampleSubpath(scene, &ctx.rng, TransportDirection::EL, maxNumVertices_);
                    subpathL.SampleSubpath(scene, &ctx.rng, TransportDirection::LE, maxNumVertices_);
                    #pragma endregion

                    // --------------------------------------------------------------------------------

                    #pragma region Combine subpaths
                    const int nE = (int)(subpathE.vertices.size());
                    for (int t = 1; t <= nE; t++)
                    {
                        #pragma region Vertex connection
                        if (mode_ == Mode::VCM || mode_ == Mode::BDPT)
                        {
                            const int nL = (int)(subpathL.vertices.size());
                            const int minS = Math::Max(0, Math::Max(2 - t, minNumVertices_ - t));
                            const int maxS = Math::Min(nL, maxNumVertices_ - t);
                            for (int s = minS; s <= maxS; s++)
                            {
                                // Connect vertices and create a full path
                                static thread_local VCMPath fullpath;
                                if (!fullpath.ConnectSubpaths(scene, subpathL, subpathE, s, t)) { continue; }

                                // Evaluate contribution
                                const auto f = fullpath.EvaluateF(s, false);
                                if (f.Black()) { continue; }

                                // Evaluate connection PDF
                                const auto p = fullpath.EvaluatePathPDF(scene, s, false, 0_f);
                                if (p.v == 0)
                                {
                                    // Due to precision issue, this can happen.
                                    return;
                                }

                                // Evaluate MIS weight
                                const auto w = mode_ == Mode::VCM
                                    ? fullpath.EvaluateMISWeight_VCM(scene, s, false, mergeRadius, numPhotonTraceSamples_)
                                    : fullpath.EvaluateMISWeight_BDPT(scene, s);

                                // Accumulate contribution
                                const auto C = f * w / p;
                                ctx.film->Splat(fullpath.RasterPosition(), C * (Float)(film->Width() * film->Height()) / (Float)numEyeTraceSamples_);
                            }
                        }
                        #pragma endregion

                        // --------------------------------------------------------------------------------

                        #pragma region Vertex merging
                        if (mode_ == Mode::VCM || mode_ == Mode::BDPM)
                        {
                            const auto& vE = subpathE.vertices[t - 1];
                            if (vE.primitive->IsDeltaPosition(vE.type))
                            {
                                continue;
                            }
                            pm.RangeQuery(vE.geom.p, mergeRadius, [&](int si, int vi) -> void
                            {
                                const int s = vi + 1;
                                const int n = s + t - 1;
                                if (n < minNumVertices_ || maxNumVertices_ < n) { return; }

                                // Merge vertices and create a full path
                                static thread_local VCMPath fullpath;
                                if (!fullpath.MergeSubpaths(subpathLs[si], subpathE, s - 1, t)) { return; }

                                // Evaluate contribution
                                const auto f = fullpath.EvaluateF(s - 1, true);
                                if (f.Black()) { return; }

                                // Evaluate path PDF
                                const auto p = fullpath.EvaluatePathPDF(scene, s - 1, true, mergeRadius);
                                if (p.v == 0)
                                {
                                    // Due to precision issue, this can happen.
                                    return;
                                }

                                // Evaluate MIS weight
                                const auto w = mode_ == Mode::VCM
                                    ? fullpath.EvaluateMISWeight_VCM(scene, s - 1, true, mergeRadius, numPhotonTraceSamples_)
                                    : fullpath.EvaluateMISWeight_BDPM(scene, s - 1, mergeRadius, numPhotonTraceSamples_);

                                // Accumulate contribution
                                const auto C = f * w / p;
                                ctx.film->Splat(fullpath.RasterPosition(), C * (Float)(film->Width() * film->Height()) / (Float)numEyeTraceSamples_);
                            });
                        }
                        #pragma endregion
                    }
                    #pragma endregion
                });

                film->Rescale((Float)(pass) / (1_f + pass));
                for (auto& ctx : contexts)
                {
                    ctx.film->Rescale(1_f / (1_f + pass));
                    film->Accumulate(ctx.film.get());
                }
            }
            #pragma endregion

            // --------------------------------------------------------------------------------

            #if LM_VCM_DEBUG
            {
                boost::format f(debugOutputPath_);
                f.exceptions(boost::io::all_error_bits ^ (boost::io::too_many_args_bit | boost::io::too_few_args_bit));
                film->Save(boost::str(f % pass));
            }
            #endif
        }
    };

};

LM_COMPONENT_REGISTER_IMPL(Renderer_VCM, "renderer::vcm");

LM_NAMESPACE_END
