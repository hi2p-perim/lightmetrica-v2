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

#if LM_SSE && LM_SINGLE_PRECISION

LM_NAMESPACE_BEGIN

struct Ray4
{
    __m128 ox, oy, oz;
    __m128 dx, dy, dz;

    Ray4(const Ray& ray)
    {
        ox = _mm_set1_ps(ray.o.x);
        oy = _mm_set1_ps(ray.o.y);
        oz = _mm_set1_ps(ray.o.z);
        dx = _mm_set1_ps(ray.d.x);
        dy = _mm_set1_ps(ray.d.y);
        dz = _mm_set1_ps(ray.d.z);
    }
};

struct QBVHNode : public SIMDAlignedType
{
    // Constant which indicates a empty leaf node
    static const int EmptyLeafNode = 0xffffffff;

    // Bounds for 4 nodes in SOA format
    __m128 bounds[2][3];

    /*
        Child nodes
        If the node is a leaf, the reference to the primitive is encoded to
            [31:31] : 1
            [30:27] : # of triangles in the leaf
            [26: 0] : An index of the first quad triangles
        If the node is a intermediate node,
            [31:31] : 0
            [30: 0] : An index of the child node
    */
    int children[4];

public:

    QBVHNode()
    {
        for (int i = 0; i < 3; i++)
        {
            bounds[0][i] = _mm_set1_ps(std::numeric_limits<float>::infinity());
            bounds[1][i] = _mm_set1_ps(-std::numeric_limits<float>::infinity());
        }
        for (int i = 0; i < 4; i++)
        {
            children[i] = EmptyLeafNode;
        }
    }

    auto SetBound(int childIndex, const Bound& bound) -> void
    {
        for (int axis = 0; axis < 3; axis++)
        {
            reinterpret_cast<float*>(&(bounds[0][axis]))[childIndex] = bound.min[axis];
            reinterpret_cast<float*>(&(bounds[1][axis]))[childIndex] = bound.max[axis];
        }
    }

    auto CreateLeaf(int childIndex, unsigned int size, unsigned int offset) -> void
    {
        if (size == 0)
        {
            children[childIndex] = EmptyLeafNode;
        }
        else
        {
            children[childIndex] = 0x80000000;
            children[childIndex] |= ((static_cast<int>(size) - 1) & 0xf) << 27;
            children[childIndex] |= static_cast<int>(offset) & 0x07ffffff;
        }
    }

    auto CreateIntermediateNode(int childIndex, unsigned int index) -> void
    {
        children[childIndex] = static_cast<int>(index);
    }

    static auto ExtractLeafData(int data, unsigned int& size, unsigned int& offset) -> void
    {
        size = static_cast<unsigned int>(((data >> 27) & 0xf) + 1);
        offset = data & 0x07ffffff;
    }

    auto Intersect(const Ray4& ray4, const __m128 invRayDirMinT[3], const __m128 invRayDirMaxT[3], const int rayDirSign[3], float _minT, float _maxT) -> int
    {
        __m128 minT = _mm_set1_ps(_minT);
        __m128 maxT = _mm_set1_ps(_maxT);
        const auto t1 = _mm_sub_ps(bounds[rayDirSign[0]][0], ray4.ox);
        const auto t2 = _mm_mul_ps(t1, invRayDirMinT[0]);
        minT = _mm_max_ps(minT, t2);
        maxT = _mm_min_ps(maxT, _mm_mul_ps(_mm_sub_ps(bounds[1 - rayDirSign[0]][0], ray4.ox), invRayDirMaxT[0]));
        minT = _mm_max_ps(minT, _mm_mul_ps(_mm_sub_ps(bounds[rayDirSign[1]][1], ray4.oy), invRayDirMinT[1]));
        maxT = _mm_min_ps(maxT, _mm_mul_ps(_mm_sub_ps(bounds[1 - rayDirSign[1]][1], ray4.oy), invRayDirMaxT[1]));
        minT = _mm_max_ps(minT, _mm_mul_ps(_mm_sub_ps(bounds[rayDirSign[2]][2], ray4.oz), invRayDirMinT[2]));
        maxT = _mm_min_ps(maxT, _mm_mul_ps(_mm_sub_ps(bounds[1 - rayDirSign[2]][2], ray4.oz), invRayDirMaxT[2]));
        return _mm_movemask_ps(_mm_cmpge_ps(maxT, minT));
    }
};

class Accel_QBVH final : public Accel3
{
public:

    LM_IMPL_CLASS(Accel_QBVH, Accel3);

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

        const std::function<void(int, int, int, int, int)> Build_ = [&](int begin, int end, int parent, int child, int depth) -> void
        {
            #pragma region Compute current bound

            Bound bound;
            for (int i = begin; i < end; i++)
            {
                bound = Math::Union(bound, bounds_[indices_[i]]);
            }

            #pragma endregion

            // --------------------------------------------------------------------------------

            #pragma region Create leaf node

            const int LeafNumNodes = 10;
            if (end - begin < LeafNumNodes)
            {
                const auto& node = nodes_[parent];
                node->SetBound(child, bound);
                node->CreateLeaf(child, end - begin, begin);
                return;
            }

            #pragma endregion

            // --------------------------------------------------------------------------------

            #pragma region Determine split position & Partition

            int mid = -1;
            {
                #pragma region Centroid bound

                Bound centroidBound;
                for (int i = begin; i < end; i++)
                {
                    centroidBound = Math::Union(centroidBound, bounds_[indices_[i]].Centroid());
                }

                #pragma endregion
        
                // --------------------------------------------------------------------------------

                #pragma region Create bins and compute SAH costs

                // Select longest axis
                int axis = centroidBound.LongestAxis();

                // Sort along the longest axis with bin sort
                const int NumBins = 100;
                Bound bounds[NumBins];
                int counts[NumBins] = {0};
                for (int i = begin; i < end; i++)
                {
                    const auto b = bounds_[indices_[i]];
                    const float min = centroidBound.min[axis];
                    const float max = centroidBound.max[axis];
                    int idx = std::min((int)((b.Centroid()[axis] - min) / (max - min) * NumBins), NumBins - 1);
                    bounds[idx] = Math::Union(bounds[idx], b);
                    counts[idx]++;
                }

                // Compute local SAH costs
                float costs[NumBins - 1];
                for (int split = 0; split < NumBins - 1; split++)
                {
                    #pragma region Compute bounds of split parts

                    Bound bound1;
                    Bound bound2;
                    int n1 = 0;
                    int n2 = 0;

                    for (int i = 0; i <= split; i++)
                    {
                        bound1 = Math::Union(bound1, bounds[i]);
                        n1 += counts[i];
                    }

                    for (int i = split + 1; i < NumBins; i++)
                    {
                        bound2 = Math::Union(bound2, bounds[i]);
                        n2 += counts[i];
                    }

                    #pragma endregion

                    #pragma region Compute local SAH cost

                    const float Cb = 0.125f;
                    const float C1 = n1 > 0 ? bound1.SurfaceArea() * n1 : 0.0f;
                    const float C2 = n2 > 0 ? bound2.SurfaceArea() * n2 : 0.0f;
                    costs[split] = Cb + (C1 + C2) / bound.SurfaceArea();

                    #pragma endregion
                }

                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Find minimum partition with minimum local cost

                const int minSplitIdx = (int)(std::distance(costs, std::min_element(costs, costs + NumBins - 1)));

                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Partition

                mid = begin + (int)(std::distance(indices_.begin() + begin, std::partition(indices_.begin() + begin, indices_.begin() + end, [&](int i) -> bool
                {
                    const auto c =  bounds_[i].Centroid()[axis];
                    const float min = centroidBound.min[axis];
                    const float max = centroidBound.max[axis];
                    int idx = std::min((int)((c - min) / (max - min) * NumBins), NumBins - 1);
                    return idx <= minSplitIdx;
                })));

                #pragma endregion
            }

            #pragma endregion

            // --------------------------------------------------------------------------------

            #pragma region Process child nodes

            {
                #pragma region Current & child node indices

                int current;
                int child1;
                int child2;

                if (depth % 2 == 1)
                {
                    #pragma region Process sibling children

                    current = parent;
                    child1 = child;
                    child2 = child + 1;

                    #pragma endregion
                }
                else
                {
                    #pragma region Create a new intermediate node

                    // Create a new node
                    current = (int)(nodes_.size());
                    nodes_.emplace_back(new QBVHNode, [](QBVHNode* p){ delete p; });

                    // Set information to parent node
                    nodes_[parent]->CreateIntermediateNode(child, current);
                    nodes_[parent]->SetBound(child, bound);

                    // Child indices
                    child1 = 0;
                    child2 = 2;

                    #pragma endregion
                }

                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Process nodes recursively

                assert(begin != mid && mid != end);
                Build_(begin, mid, current, child1, depth + 1);
                Build_(mid, end, current, child2, depth + 1);

                #pragma endregion
            }

            #pragma endregion
        };

        nodes_.clear();
        indices_.assign(triangles_.size(), 0);
        std::iota(indices_.begin(), indices_.end(), 0);
        nodes_.emplace_back(new QBVHNode, [](QBVHNode* p) { delete p; });
        Build_(0, (int)(triangles_.size()), 0, 0, 0);

        #pragma endregion

        // --------------------------------------------------------------------------------

        return true;
    };

    LM_IMPL_F(Intersect) = [this](const Scene* scene_, const Ray& ray, Intersection& isect, Float minT, Float maxT) -> bool
    {
        #pragma region Prepare some required data

        bool hit = false;
        int minIndex;
        Vec2 minB;

        Ray4 ray4(ray);
        __m128 invRayDirMinT[3];
        __m128 invRayDirMaxT[3];
        int rayDirSign[3];

        invRayDirMinT[0] = _mm_set1_ps(ray.d.x == 0.0f ? Math::EpsLarge() : 1.0f / ray.d.x);
        invRayDirMinT[1] = _mm_set1_ps(ray.d.y == 0.0f ? Math::EpsLarge() : 1.0f / ray.d.y);
        invRayDirMinT[2] = _mm_set1_ps(ray.d.z == 0.0f ? Math::EpsLarge() : 1.0f / ray.d.z);
        invRayDirMaxT[0] = _mm_set1_ps(ray.d.x == 0.0f ? Math::Inf()      : 1.0f / ray.d.x);
        invRayDirMaxT[1] = _mm_set1_ps(ray.d.y == 0.0f ? Math::Inf()      : 1.0f / ray.d.y);
        invRayDirMaxT[2] = _mm_set1_ps(ray.d.z == 0.0f ? Math::Inf()      : 1.0f / ray.d.z);

        rayDirSign[0] = ray.d.x < 0.0f;
        rayDirSign[1] = ray.d.y < 0.0f;
        rayDirSign[2] = ray.d.z < 0.0f;

        #pragma endregion

        // --------------------------------------------------------------------------------

        #pragma region Traverse BVH

        // Stack for traversal
        const int StackSize = 64;
        int stack[StackSize];
        int stackIndex = 0;

        // Initial state
        stack[0] = 0;

        while (stackIndex >= 0)
        {
            int data = stack[stackIndex--];
            if (data < 0)
            {
                #pragma region Leaf node
            
                // If the node is empty, ignore it
                if (data == QBVHNode::EmptyLeafNode)
                {
                    continue;
                }

                // Intersection with objects
                unsigned int size, offset;
                QBVHNode::ExtractLeafData(data, size, offset);
                for (unsigned int i = offset; i < offset + size; i++)
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

                #pragma endregion
            }
            else
            {
                #pragma region Intermediate node

                const auto& node = nodes_[data];
                int mask = node->Intersect(ray4, invRayDirMinT, invRayDirMaxT, rayDirSign, minT, maxT);
                if (mask & 0x1) stack[++stackIndex] = node->children[0];
                if (mask & 0x2) stack[++stackIndex] = node->children[1];
                if (mask & 0x4) stack[++stackIndex] = node->children[2];
                if (mask & 0x8) stack[++stackIndex] = node->children[3];

                #pragma endregion
            }
        }

        #pragma endregion

        // --------------------------------------------------------------------------------

        if (hit)
        {
            const auto* scene = static_cast<const Scene3*>(scene_);
            isect = IntersectionUtils::CreateTriangleIntersection(
                scene->PrimitiveAt(triangles_[minIndex].primIndex),
                ray.o + ray.d * maxT,
                minB,
                triangles_[minIndex].faceIndex);
        }

        return hit;
    };

private:

    std::vector<TriAccelTriangle> triangles_;
    std::vector<std::unique_ptr<QBVHNode, std::function<void(QBVHNode*)>>> nodes_;
    std::vector<int> indices_;

};

LM_COMPONENT_REGISTER_IMPL(Accel_QBVH, "accel::qbvh");

LM_NAMESPACE_END

#endif