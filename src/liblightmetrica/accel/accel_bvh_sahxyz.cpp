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
#include <lightmetrica/accel3.h>
#include <lightmetrica/scene3.h>
#include <lightmetrica/trianglemesh.h>
#include <lightmetrica/triaccel.h>
#include <lightmetrica/primitive.h>
#include <lightmetrica/bound.h>
#include <lightmetrica/intersectionutils.h>

LM_NAMESPACE_BEGIN

struct BVHNode
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

class Accel_BVHSAHXYZ final : public Accel3
{
public:

    LM_IMPL_CLASS(Accel_BVHSAHXYZ, Accel3);

public:

    LM_IMPL_F(Initialize) = [this](const PropertyNode*) -> bool
    {
        return true;
    };

    LM_IMPL_F(Build) = [this](const Scene* scene_) -> bool
    {
        std::vector<Bound> bounds_;
        const auto* scene = static_cast<const Scene3*>(scene_);

        // --------------------------------------------------------------------------------

        #pragma region Create triaccels

        int np = scene->NumPrimitives();
        for (int i = 0; i < np; i++)
        {
            const auto* prim = scene->PrimitiveAt(i);
            const auto* mesh = prim->mesh;
            if (mesh)
            {
                // Enumerate all triangles and create triaccels
                const auto* ps = mesh->Positions();
                const auto* faces = mesh->Faces();
                for (int j = 0; j < mesh->NumFaces(); j++)
                {
                    // Create a triaccel
                    triangles_.push_back(TriAccelTriangle());
                    triangles_.back().faceIndex = j;
                    triangles_.back().primIndex = i;
                    unsigned int i1 = faces[3 * j];
                    unsigned int i2 = faces[3 * j + 1];
                    unsigned int i3 = faces[3 * j + 2];
                    Vec3 p1(prim->transform * Vec4(ps[3 * i1], ps[3 * i1 + 1], ps[3 * i1 + 2], 1_f));
                    Vec3 p2(prim->transform * Vec4(ps[3 * i2], ps[3 * i2 + 1], ps[3 * i2 + 2], 1_f));
                    Vec3 p3(prim->transform * Vec4(ps[3 * i3], ps[3 * i3 + 1], ps[3 * i3 + 2], 1_f));
                    triangles_.back().Load(p1, p2, p3);

                    Bound bound;
                    bound = Math::Union(bound, p1);
                    bound = Math::Union(bound, p2);
                    bound = Math::Union(bound, p3);
                    bound.min -= Vec3(Math::Eps());
                    bound.max += Vec3(Math::Eps());
                    bounds_.push_back(bound);
                }
            }
        }

        #pragma endregion

        // --------------------------------------------------------------------------------

        #pragma region Build BVH

        const std::function<int(int, int)> Build_ = [&](int begin, int end) -> int
        {
            int idx = (int)(nodes_.size());
            nodes_.emplace_back(new BVHNode);
            auto* node = nodes_[idx].get();

            // Current bound
            node->bound = Bound();
            for (int i = begin; i < end; i++)
            {
                node->bound = Math::Union(node->bound, bounds_[indices_[i]]);
            }

            // Leaf node
            const int LeafNumNodes = 10;
            if (end - begin < LeafNumNodes)
            {
                node->isleaf = true;
                node->leaf.begin = begin;
                node->leaf.end = end;
                return idx;
            }

            // Determine split index
            int mid = 0;
            {
                // Select longest axis
                int axis = node->bound.LongestAxis();

                // Sort along the longest axis
                std::sort(indices_.begin() + begin, indices_.begin() + end, [&](int v1, int v2) -> bool
                {
                    return bounds_[v1].Centroid()[axis] < bounds_[v2].Centroid()[axis];
                });

                // Compute local SAH costs
                const int NumSplitCandidates = end - begin - 1;

                std::vector<Bound> sumLeft(NumSplitCandidates);
                for (int i = begin; i < end - 1; i++)
                {
                    const auto b = bounds_[indices_[i]];
                    const int split = i - begin;
                    if (split == 0)
                    {
                        sumLeft[split] = b;
                    }
                    else
                    {
                        sumLeft[split] = Math::Union(sumLeft[split - 1], b);
                    }
                }

                std::vector<Bound> sumRight(NumSplitCandidates + 1);
                for (int i = end - 1; i > begin; i--)
                {
                    const auto b = bounds_[indices_[i]];
                    const int split = i - begin;
                    if (split == NumSplitCandidates - 1)
                    {
                        sumRight[split] = b;
                    }
                    else
                    {
                        sumRight[split] = Math::Union(sumRight[split + 1], b);
                    }
                }

                std::vector<Float> costs(NumSplitCandidates);
                for (int split = 0; split < NumSplitCandidates; split++)
                {
                    const auto& bound1 = sumLeft[split];
                    const auto& bound2 = sumRight[split + 1];

                    // Compute local SAH cost
                    const Float Cb = 0.125_f;
                    const int n1 = split + 1;
                    const int n2 = end - begin - split - 1;
                    costs[split] = Cb + (bound1.SurfaceArea() * n1 + bound2.SurfaceArea() * n2) / node->bound.SurfaceArea();
                }

                // Select split position with minimum local cost
                const int split = (int)(std::distance(costs.begin(), std::min_element(costs.begin(), costs.end())));
                mid = begin + split + 1;
            }

            // Intermediate node
            node->isleaf = false;
            node->internal.child1 = Build_(begin, mid);
            node->internal.child2 = Build_(mid, end);
            return idx;
        };

        nodes_.clear();
        indices_.assign(triangles_.size(), 0);
        std::iota(indices_.begin(), indices_.end(), 0);
        Build_(0, (int)(triangles_.size()));

        #pragma endregion

        // --------------------------------------------------------------------------------

        return true;
    };

    LM_IMPL_F(Intersect) = [this](const Scene* scene_, const Ray& ray, Intersection& isect, Float minT, Float maxT) -> bool
    {
        int minIndex;
        Vec2 minB;

        const std::function<bool(int)> Intersect_ = [&](int idx) -> bool
        {
            const auto* node = nodes_.at(idx).get();

            // Check intersection with bound
            if (!node->bound.Intersect(ray, minT, maxT))
            {
                return false;
            }

            // Check intersection with objects in the leaf
            if (node->isleaf)
            {
                bool hit = false;
                for (int i = node->leaf.begin; i < node->leaf.end; i++)
                {
                    Float t;
                    Vec2 b;
                    if (triangles_[indices_[i]].Intersect(ray, minT, maxT, b[0], b[1], t))
                    {
                        hit = true;
                        maxT = t;
                        minIndex = indices_[i];
                        minB = b;
                    }
                }
                return hit;
            }

            // Check intersection with child nodes
            bool hit = false;
            hit |= Intersect_(node->internal.child1);
            hit |= Intersect_(node->internal.child2);
            return hit;
        };

        if (!Intersect_(0))
        {
            return false;
        }

        const auto* scene = static_cast<const Scene3*>(scene_);
        isect = IntersectionUtils::CreateTriangleIntersection(
            scene->PrimitiveAt(triangles_[minIndex].primIndex),
            ray.o + ray.d * maxT,
            minB,
            triangles_[minIndex].faceIndex);

        return true;
    };

private:

    std::vector<TriAccelTriangle> triangles_;
    std::vector<std::unique_ptr<BVHNode>> nodes_;
    std::vector<int> indices_;                      // Triangle indices

};

LM_COMPONENT_REGISTER_IMPL(Accel_BVHSAHXYZ, "accel::bvh_sahxyz");

LM_NAMESPACE_END
