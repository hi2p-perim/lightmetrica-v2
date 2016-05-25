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

#include <lightmetrica/detail/photonmaputils.h>
#include <lightmetrica/surfaceinteraction.h>
#include <lightmetrica/surfacegeometry.h>
#include <lightmetrica/bound.h>
#include <functional>
#include <vector>

LM_NAMESPACE_BEGIN

struct PathVertex
{
    int type;
    SurfaceGeometry geom;
    const Primitive* primitive = nullptr;
};

class Scene;
class Random;

struct Subpath
{
    std::vector<PathVertex> vertices;
    LM_PUBLIC_API auto SampleSubpath(const Scene* scene, Random* rng, TransportDirection transDir, int maxNumVertices) -> void;
};

struct Path
{
    std::vector<PathVertex> vertices;
    LM_PUBLIC_API auto ConnectSubpaths(const Scene* scene, const Subpath& subpathL, const Subpath& subpathE, int s, int t) -> bool;
    LM_PUBLIC_API auto MergeSubpaths(const Subpath& subpathL, const Subpath& subpathE, int s, int t) -> bool;
    LM_PUBLIC_API auto EvaluateF(int s, bool merge) const->SPD;
    LM_PUBLIC_API auto EvaluatePathPDF(const Scene* scene, int s, bool merge, Float radius) const->PDFVal;
    LM_PUBLIC_API auto EvaluateMISWeight_VCM(const Scene* scene, int s_, bool merge, Float radius, long long numPhotonTraceSamples) const -> Float;
    LM_PUBLIC_API auto EvaluateMISWeight_BDPT(const Scene* scene, int s_) const -> Float;
    LM_PUBLIC_API auto EvaluateMISWeight_BDPM(const Scene* scene, int s_, Float radius, long long numPhotonTraceSamples) const -> Float;
    LM_PUBLIC_API auto RasterPosition() const->Vec2;
};

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
    const std::vector<Subpath>& subpathLs_;

    LM_PUBLIC_API VCMKdTree(const std::vector<Subpath>& subpathLs);
    LM_PUBLIC_API auto RangeQuery(const Vec3& p, Float radius, const std::function<void(int subpathIndex, int vertexIndex)>& queryFunc) const -> void;
};

LM_NAMESPACE_END
