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
#include <lightmetrica/detail/vcmutils.h>
#include <lightmetrica/primitive.h>
#include <lightmetrica/scene.h>
#include <lightmetrica/random.h>
#include <lightmetrica/sensor.h>

LM_NAMESPACE_BEGIN

auto Subpath::SampleSubpath(const Scene* scene, Random* rng, TransportDirection transDir, int maxNumVertices) -> void
{
    vertices.clear();
    PhotonMapUtils::TraceSubpath(scene, rng, maxNumVertices, transDir, [&](int numVertices, const Vec2& /*rasterPos*/, const PhotonMapUtils::PathVertex& pv, const PhotonMapUtils::PathVertex& v, SPD& throughput) -> bool
    {
        PathVertex v_;
        v_.type = v.type;
        v_.geom = v.geom;
        v_.primitive = v.primitive;
        vertices.emplace_back(v_);
        return true;
    });
}

// --------------------------------------------------------------------------------

auto Path::ConnectSubpaths(const Scene* scene, const Subpath& subpathL, const Subpath& subpathE, int s, int t) -> bool
{
    assert(s >= 0);
    assert(t >= 0);
    vertices.clear();
    if (s == 0 && t > 0)
    {
        vertices.insert(vertices.end(), subpathE.vertices.rbegin(), subpathE.vertices.rend());
        if ((vertices.front().primitive->surface->Type() & SurfaceInteractionType::L) == 0) { return false; }
        vertices.front().type = SurfaceInteractionType::L;
    }
    else if (s > 0 && t == 0)
    {
        vertices.insert(vertices.end(), subpathL.vertices.begin(), subpathL.vertices.end());
        if ((vertices.back().primitive->surface->Type() & SurfaceInteractionType::E) == 0) { return false; }
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

auto Path::MergeSubpaths(const Subpath& subpathL, const Subpath& subpathE, int s, int t) -> bool
{
    assert(s >= 1);
    assert(t >= 1);
    vertices.clear();
    const auto& vL = subpathL.vertices[s - 1];
    const auto& vE = subpathE.vertices[t - 1];
    if (vL.primitive->surface->IsDeltaPosition(vL.type) || vE.primitive->surface->IsDeltaPosition(vE.type)) { return false; }
    if (vL.geom.infinite || vE.geom.infinite) { return false; }
    vertices.insert(vertices.end(), subpathL.vertices.begin(), subpathL.vertices.begin() + s);
    vertices.insert(vertices.end(), subpathE.vertices.rend() - t, subpathE.vertices.rend());
    return true;
}

auto Path::EvaluateF(int s, bool merge) const -> SPD
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
            fL = vL->primitive->emitter->EvaluatePosition(vL->geom, false);
        }
        for (int i = 0; i < (merge ? s : s - 1); i++)
        {
            const auto* v = &vertices[i];
            const auto* vPrev = i >= 1 ? &vertices[i - 1] : nullptr;
            const auto* vNext = &vertices[i + 1];
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
            const auto* vE = &vertices[n - 1];
            fE = vE->primitive->emitter->EvaluatePosition(vE->geom, false);
        }
        for (int i = n - 1; i > s; i--)
        {
            const auto* v = &vertices[i];
            const auto* vPrev = &vertices[i - 1];
            const auto* vNext = i < n - 1 ? &vertices[i + 1] : nullptr;
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
            const auto& v = vertices[0];
            const auto& vNext = vertices[1];
            cst = v.primitive->emitter->EvaluatePosition(v.geom, true) * v.primitive->emitter->EvaluateDirection(v.geom, v.type, Vec3(), Math::Normalize(vNext.geom.p - v.geom.p), TransportDirection::EL, false);
        }
        else if (s > 0 && t == 0)
        {
            const auto& v = vertices[n - 1];
            const auto& vPrev = vertices[n - 2];
            cst = v.primitive->emitter->EvaluatePosition(v.geom, true) * v.primitive->emitter->EvaluateDirection(v.geom, v.type, Vec3(), Math::Normalize(vPrev.geom.p - v.geom.p), TransportDirection::LE, false);
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
    }
    else
    {
        assert(s >= 1);
        assert(t >= 1);
        const auto& v     = vertices[s];
        const auto& vPrev = vertices[s-1];
        const auto& vNext = vertices[s+1];
        const auto fs = v.primitive->surface->EvaluateDirection(v.geom, v.type, Math::Normalize(vPrev.geom.p - v.geom.p), Math::Normalize(vNext.geom.p - v.geom.p), TransportDirection::LE, false);
        cst = fs;
    }

    // --------------------------------------------------------------------------------

    return fL * cst * fE;
}

auto Path::EvaluatePathPDF(const Scene* scene, int s, bool merge, Float radius) const -> PDFVal
{
    const int n = (int)(vertices.size());
    const int t = n - s;
    assert(n >= 2);
    assert(n <= maxNumVertices_);

    if (!merge)
    {
        // Check if the path is samplable by vertex connection
        if (s == 0 && t > 0)
        {
            const auto& v = vertices[0];
            if (v.primitive->emitter->IsDeltaPosition(v.type)) { return PDFVal(PDFMeasure::ProdArea, 0_f); }
        }
        else if (s > 0 && t == 0)
        {
            const auto& v = vertices[n - 1];
            if (v.primitive->emitter->IsDeltaPosition(v.type)) { return PDFVal(PDFMeasure::ProdArea, 0_f); }
        }
        else if (s > 0 && t > 0)
        {
            const auto& vL = vertices[s - 1];
            const auto& vE = vertices[s];
            if (vL.primitive->surface->IsDeltaDirection(vL.type) || vE.primitive->surface->IsDeltaDirection(vE.type)) { return PDFVal(PDFMeasure::ProdArea, 0_f); }
        }
    }
    else
    {
        // Check if the path is samplable by vertex merging
        if (s == 0 || t == 0) { return PDFVal(PDFMeasure::ProdArea, 0_f); }
        const auto& vE = vertices[s];
        if (vE.primitive->surface->IsDeltaPosition(vE.type) || vE.primitive->surface->IsDeltaDirection(vE.type)) { return PDFVal(PDFMeasure::ProdArea, 0_f); }
    }

    // Otherwise the path can be generated with the given strategy (s,t,merge) so p_{s,t,merge} can be safely evaluated.
    PDFVal pdf(PDFMeasure::ProdArea, 1_f);
    if (s > 0)
    {
        pdf *= vertices[0].primitive->emitter->EvaluatePositionGivenDirectionPDF(vertices[0].geom, Math::Normalize(vertices[1].geom.p - vertices[0].geom.p), false) * scene->EvaluateEmitterPDF(vertices[0].primitive).v;
        for (int i = 0; i < (merge ? s : s - 1); i++)
        {
            const auto* vi = &vertices[i];
            const auto* vip = i - 1 >= 0 ? &vertices[i - 1] : nullptr;
            const auto* vin = &vertices[i + 1];
            pdf *= vi->primitive->surface->EvaluateDirectionPDF(vi->geom, vi->type, vip ? Math::Normalize(vip->geom.p - vi->geom.p) : Vec3(), Math::Normalize(vin->geom.p - vi->geom.p), false).ConvertToArea(vi->geom, vin->geom);
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
            pdf *= vi->primitive->surface->EvaluateDirectionPDF(vi->geom, vi->type, vin ? Math::Normalize(vin->geom.p - vi->geom.p) : Vec3(), Math::Normalize(vip->geom.p - vi->geom.p), false).ConvertToArea(vi->geom, vip->geom);
        }
    }

    if (merge)
    {
        pdf.v *= (Math::Pi() * radius * radius);
    }

    return pdf;
}

auto Path::EvaluateMISWeight_VCM(const Scene* scene, int s_, bool merge, Float radius, long long numPhotonTraceSamples) const -> Float
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

auto Path::EvaluateMISWeight_BDPT(const Scene* scene, int s_) const -> Float
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

auto Path::EvaluateMISWeight_BDPM(const Scene* scene, int s_, Float radius, long long numPhotonTraceSamples) const -> Float
{
    const int n = static_cast<int>(vertices.size());
    const auto ps = EvaluatePathPDF(scene, s_, true, radius);
    assert(ps > 0_f);

    Float invw = 0_f;
    for (int s = 0; s <= n; s++)
    {
        for (int type = 0; type < 2; type++)
        {
            const auto pi = EvaluatePathPDF(scene, s, true, radius);
            if (pi > 0_f)
            {
                const auto r = pi.v / ps.v;
                invw += r*r*(Float)(numPhotonTraceSamples);
            }
        }
    }

    return 1_f / invw;
}

auto Path::RasterPosition() const -> Vec2
{
    const auto& v = vertices[vertices.size() - 1];
    const auto& vPrev = vertices[vertices.size() - 2];
    Vec2 rasterPos;
    v.primitive->sensor->RasterPosition(Math::Normalize(vPrev.geom.p - v.geom.p), v.geom, rasterPos);
    return rasterPos;
}

// --------------------------------------------------------------------------------

VCMKdTree::VCMKdTree(const std::vector<Subpath>& subpathLs)
    : subpathLs_(subpathLs)
{
    // Arrange in a vector
    for (int i = 0; i < (int)subpathLs.size(); i++)
    {
        const auto& subpathL = subpathLs[i];
        for (int j = 1; j < (int)subpathL.vertices.size(); j++)
        {
            const auto& v = subpathL.vertices[j];
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
            node->bound = Math::Union(node->bound, subpathLs[v.subpathIndex].vertices[v.vertexIndex].geom.p);
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
            return subpathLs[v.subpathIndex].vertices[v.vertexIndex].geom.p[axis] < split;
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

auto VCMKdTree::RangeQuery(const Vec3& p, Float radius, const std::function<void(int subpathIndex, int vertexIndex)>& queryFunc) const -> void
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

LM_NAMESPACE_END
