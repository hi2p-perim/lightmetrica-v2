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

#include <lightmetrica/math.h>
#include <lightmetrica/scene3.h>
#include <lightmetrica/primitive.h>
#include <lightmetrica/trianglemesh.h>
#include <lightmetrica/light.h>
#include <lightmetrica/bsdf.h>
#include <lightmetrica/intersection.h>
#include <vector>
#include <stack>
#include <unordered_map>

LM_NAMESPACE_BEGIN

struct Patch
{
    const Primitive* primitive;
    Vec3 p1, p2, p3;
    Vec3 gn;
    auto Centroid() const -> Vec3 { return (p1 + p2 + p3) / 3_f; }
    auto Area() const -> Float { return Math::Length(Math::Cross(p2 - p1, p3 - p1)) * 0.5_f; }
};

struct Index
{
    int primitiveIndex;
    int faceIndex;
};

///! Set of patches.
class Patches
{
private:
    
    const std::function<size_t(const Index&)> Hash = [this](const Index& index) -> size_t
    {
        assert(index.primitiveIndex >= 0);
        assert(psum_[index.primitiveIndex + 1] - psum_[index.primitiveIndex] > index.faceIndex);
        return psum_[index.primitiveIndex] + index.faceIndex;
    };
    const std::function<bool(const Index&, const Index&)> IndexEq = [](const Index& lhs, const Index& rhs) -> bool
    {
        return lhs.primitiveIndex == rhs.primitiveIndex && lhs.faceIndex == rhs.faceIndex;
    };
    std::vector<Patch> patches_;
    std::vector<int> psum_;
    std::unordered_map<Index, size_t, decltype(Hash), decltype(IndexEq)> patchIndexMap_{ 10, Hash, IndexEq };   

public:

    ///! Get the patch at index `i`
    auto At(size_t i) const -> const Patch& { return patches_.at(i); }

    ///! Get the size of the patches
    auto Size() const -> int { return (int)(patches_.size()); }

    ///! Create patches structure by subdividing the triangles meshes
    auto Create(const Scene3* scene, Float subdivLimitArea) -> void
    {
        LM_LOG_INFO("Creating patches");

        // --------------------------------------------------------------------------------

        // Define Hash function for index
        psum_.assign(scene->NumPrimitives() + 1, 0);
        psum_[0] = 0;
        for (int i = 0; i < scene->NumPrimitives(); i++)
        {
            const auto* primitive = scene->PrimitiveAt(i);
            psum_[i + 1] = psum_[i] + (primitive->mesh ? primitive->mesh->NumFaces() : 0);
        }

        // --------------------------------------------------------------------------------

        patchIndexMap_.clear();
        for (int i = 0; i < scene->NumPrimitives(); i++)
        {
            const auto* primitive = scene->PrimitiveAt(i);
            const auto* mesh = primitive->mesh;
            if (!mesh)
            {
                continue;
            }

            // --------------------------------------------------------------------------------

            #pragma region Check validity of the primitive

            if (primitive->light)
            {
                if (std::strcmp(primitive->light->implName, "Light_Area") != 0)
                {
                    LM_LOG_WARN("Non area light is found; skipping.");
                    continue;
                }
            }
            if (primitive->bsdf)
            {
                if (std::strcmp(primitive->bsdf->implName, "BSDF_Diffuse") != 0)
                {
                    LM_LOG_WARN("Non diffuse BSDF is found; skipping.");
                    continue;
                }
            }

            #pragma endregion

            // --------------------------------------------------------------------------------

            #pragma region Subdivide the triangles in the mesh

            const auto* ps = mesh->Positions();
            const auto* faces = mesh->Faces();
            for (int fi = 0; fi < primitive->mesh->NumFaces(); fi++)
            {
                // Transformed triangle vertices and geometric normal
                unsigned int vi1 = faces[3 * fi];
                unsigned int vi2 = faces[3 * fi + 1];
                unsigned int vi3 = faces[3 * fi + 2];
                Vec3 p1(primitive->transform * Vec4(ps[3 * vi1], ps[3 * vi1 + 1], ps[3 * vi1 + 2], 1_f));
                Vec3 p2(primitive->transform * Vec4(ps[3 * vi2], ps[3 * vi2 + 1], ps[3 * vi2 + 2], 1_f));
                Vec3 p3(primitive->transform * Vec4(ps[3 * vi3], ps[3 * vi3 + 1], ps[3 * vi3 + 2], 1_f));
                const auto gn = Math::Normalize(Math::Cross(p2 - p1, p3 - p1));
                
                // Record current patch index associated to the primitive and face indices
                patchIndexMap_[{ i, fi }] = patches_.size();

                // --------------------------------------------------------------------------------

                #pragma region Subdivide the triangle

                struct Tri
                {
                    Vec3 p1, p2, p3;
                    auto Area() const -> Float { return Math::Length(Math::Cross(p2 - p1, p3 - p1)) * 0.5_f; }
                };

                std::stack<Tri> stack;
                stack.push({ p1, p2, p3 });
                while (!stack.empty())
                {
                    const auto tri = stack.top(); stack.pop();
                    if (tri.Area() < subdivLimitArea)
                    {
                        patches_.push_back({ primitive, tri.p1, tri.p2, tri.p3, gn });
                        continue;
                    }

                    const auto c1 = (tri.p1 + tri.p2) * 0.5_f;
                    const auto c2 = (tri.p2 + tri.p3) * 0.5_f;
                    const auto c3 = (tri.p3 + tri.p1) * 0.5_f;
                    stack.push({ tri.p1, c1, c3 });
                    stack.push({ tri.p2, c2, c1 });
                    stack.push({ tri.p3, c3, c2 });
                    stack.push({ c1, c2, c3 });
                }

                #pragma endregion
            }

            #pragma endregion
        }
    }

    ///! Iterate patch structure
    auto IteratePatches(const Intersection& isect, Float subdivLimitArea, const std::function<void(size_t patchindex, const Vec2& uv)>& iterateFunc) -> void
    {
        // For this requirement, what we want to do here is to subdivide and
        // check if the query point is in the triangle.This actually increase the complexity
        // compared with the case that we use a quad - tree associated with each triangle for example,
        // but I think the ray casting is fast enough and this does not degrade performance so much.

        // --------------------------------------------------------------------------------

        const auto* mesh = isect.primitive->mesh;
        const auto* fs = mesh->Faces();
        const auto* ps = mesh->Positions();
        int v1 = fs[3 * isect.geom.faceindex];
        int v2 = fs[3 * isect.geom.faceindex + 1];
        int v3 = fs[3 * isect.geom.faceindex + 2];
        Vec3 p1(isect.primitive->transform * Vec4(ps[3 * v1], ps[3 * v1 + 1], ps[3 * v1 + 2], 1_f));
        Vec3 p2(isect.primitive->transform * Vec4(ps[3 * v2], ps[3 * v2 + 1], ps[3 * v2 + 2], 1_f));
        Vec3 p3(isect.primitive->transform * Vec4(ps[3 * v3], ps[3 * v3 + 1], ps[3 * v3 + 2], 1_f));

        // --------------------------------------------------------------------------------

        struct Tri
        {
            Vec3 p1, p2, p3;
            auto Area() const -> Float { return Math::Length(Math::Cross(p2 - p1, p3 - p1)) * 0.5_f; }
        };

        size_t patchindex = patchIndexMap_[{ (int)(isect.primitive->index), isect.geom.faceindex }];
        std::stack<Tri> stack;
        stack.push({ p1, p2, p3 });
        while (!stack.empty())
        {
            const auto tri = stack.top(); stack.pop();
            if (tri.Area() < subdivLimitArea)
            {
                // Check if the intersected point is in the subdivided triangle
                const auto e0 = tri.p3 - tri.p1;
                const auto e1 = tri.p2 - tri.p1;
                const auto e2 = isect.geom.p - tri.p1;
                const auto dot00 = Math::Dot(e0, e0);
                const auto dot01 = Math::Dot(e0, e1);
                const auto dot02 = Math::Dot(e0, e2);
                const auto dot11 = Math::Dot(e1, e1);
                const auto dot12 = Math::Dot(e1, e2);
                const auto invDenom = 1_f / (dot00 * dot11 - dot01 * dot01);
                const auto u = (dot11 * dot02 - dot01 * dot12) * invDenom;
                const auto v = (dot00 * dot12 - dot01 * dot02) * invDenom;
                if (0_f < u && 0_f < v && u + v < 1_f)
                {
                    iterateFunc(patchindex, Vec2(u, v));
                }

                patchindex++;
                continue;
            }

            const auto c1 = (tri.p1 + tri.p2) * 0.5_f;
            const auto c2 = (tri.p2 + tri.p3) * 0.5_f;
            const auto c3 = (tri.p3 + tri.p1) * 0.5_f;
            stack.push({ tri.p1, c1, c3 });
            stack.push({ tri.p2, c2, c1 });
            stack.push({ tri.p3, c3, c2 });
            stack.push({ c1, c2, c3 });
        }
    }
    

};

///! Utility class for radiosity algorithms
class RadiosityUtils
{
public:

    LM_DISABLE_CONSTRUCT(RadiosityUtils);

public:

    ///! Helper function to estimate the form factor
    static auto EstimateFormFactor(const Scene3* scene, const Patch& pi, const Patch& pj) -> Float
    {
        const auto  ci = pi.Centroid();
        const auto  cj = pj.Centroid();

        // Check if two patches area facing each other
        const auto cij = Math::Normalize(cj - ci);
        const auto cji = -cij;
        const auto cosThetai = Math::Dot(pi.gn, cij);
        const auto cosThetaj = Math::Dot(pj.gn, cji);
        if (cosThetai <= 0_f || cosThetaj <= 0_f)
        {
            return 0_f;
        }

        // Check visibility
        if (!scene->Visible(ci, cj))
        {
            return 0_f;
        }

        #if 0
        if (analytical)
        {
            // Analytical solution [Schroder & Hanrahan 1993]
            double p[3][3] =
            {
                { pi.p1.x, pi.p1.y, pi.p1.z },
                { pi.p2.x, pi.p2.y, pi.p2.z },
                { pi.p3.x, pi.p3.y, pi.p3.z },
            };
            double q[3][3] =
            {
                { pj.p1.x, pj.p1.y, pj.p1.z },
                { pj.p2.x, pj.p2.y, pj.p2.z },
                { pj.p3.x, pj.p3.y, pj.p3.z },
            };
            return (Float)(FormFactor(&(p[0]), 3, &(q[0]), 3));
        }
        #endif

        // Point-to-point estimate (Eq.4 in [Willmott & Heckbert 1997])
        return pj.Area() * cosThetai * cosThetaj / Math::Pi() / Math::Length2(ci - cj);
    };
    
};

LM_NAMESPACE_END
