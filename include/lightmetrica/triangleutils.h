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

#include <lightmetrica/dist.h>
#include <lightmetrica/primitive.h>
#include <lightmetrica/trianglemesh.h>
#include <lightmetrica/sampler.h>

LM_NAMESPACE_BEGIN

/*!
    \brief Utility function for triangle meshes.
    \ingroup core
*/
class TriangleUtils
{
public:

    LM_DISABLE_CONSTRUCT(TriangleUtils);

public:

    //! Create discrete distribution for sampling area light or raw sensor
    static auto CreateTriangleAreaDist(const Primitive* primitive, Distribution1D& dist, Float& invArea) -> void
    {
        assert(primitive->mesh);
        Float sumArea = 0;
        dist.Clear();
        const auto* fs = primitive->mesh->Faces();
        const auto* ps = primitive->mesh->Positions();
        for (int i = 0; i < primitive->mesh->NumFaces(); i++)
        {
            unsigned int i1 = fs[3 * i];
            unsigned int i2 = fs[3 * i + 1];
            unsigned int i3 = fs[3 * i + 2];
            Vec3 p1(primitive->transform * Vec4(ps[3 * i1], ps[3 * i1 + 1], ps[3 * i1 + 2], 1_f));
            Vec3 p2(primitive->transform * Vec4(ps[3 * i2], ps[3 * i2 + 1], ps[3 * i2 + 2], 1_f));
            Vec3 p3(primitive->transform * Vec4(ps[3 * i3], ps[3 * i3 + 1], ps[3 * i3 + 2], 1_f));
            const Float area = Math::Length(Math::Cross(p2 - p1, p3 - p1)) * 0.5_f;
            dist.Add(area);
            sumArea += area;
        }
        dist.Normalize();
        invArea = 1_f / sumArea;
    }

    //! Sample a position on the triangle mesh
    static auto SampleTriangleMesh(const Vec2& u, const TriangleMesh* mesh, const Mat4& transform, const Distribution1D& dist, SurfaceGeometry& geom)
    {
        #pragma region Sample a triangle & a position on triangle

        auto u2 = u;
        const int i = dist.SampleReuse(u.x, u2.x);
        const auto b = Sampler::UniformSampleTriangle(u2);

        #pragma endregion

        // --------------------------------------------------------------------------------

        #pragma region Store surface geometry information

        const auto* fs = mesh->Faces();
        const auto* ps = mesh->Positions();
        unsigned int i1 = fs[3 * i];
        unsigned int i2 = fs[3 * i + 1];
        unsigned int i3 = fs[3 * i + 2];
        geom.faceindex = i;

        // Position
        Vec3 p1(transform * Vec4(ps[3 * i1], ps[3 * i1 + 1], ps[3 * i1 + 2], 1_f));
        Vec3 p2(transform * Vec4(ps[3 * i2], ps[3 * i2 + 1], ps[3 * i2 + 2], 1_f));
        Vec3 p3(transform * Vec4(ps[3 * i3], ps[3 * i3 + 1], ps[3 * i3 + 2], 1_f));
        geom.p = p1 * (1_f - b.x - b.y) + p2 * b.x + p3 * b.y;

        // UV
        const auto* tc = mesh->Texcoords();
        if (tc)
        {
            Vec2 uv1(tc[2 * i1], tc[2 * i1 + 1]);
            Vec2 uv2(tc[2 * i2], tc[2 * i2 + 1]);
            Vec2 uv3(tc[2 * i3], tc[2 * i3 + 1]);
            geom.uv = uv1 * (1_f - b.x - b.y) + uv2 * b.x + uv3 * b.y;
        }

        // Normal
        geom.degenerated = false;
        geom.gn = Math::Normalize(Math::Cross(p2 - p1, p3 - p1));
        geom.sn = geom.gn;

        // Compute tangent space
        Math::OrthonormalBasis(geom.sn, geom.dpdu, geom.dpdv);
        geom.ToWorld = Mat3(geom.dpdu, geom.dpdv, geom.sn);
        geom.ToLocal = Math::Transpose(geom.ToWorld);

        // Normal derivatives
        geom.dndu = geom.dndv = Vec3();

        #pragma endregion
    }

};

LM_NAMESPACE_END
