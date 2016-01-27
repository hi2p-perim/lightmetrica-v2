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

#include <lightmetrica/intersection.h>
#include <lightmetrica/primitive.h>
#include <lightmetrica/trianglemesh.h>

LM_NAMESPACE_BEGIN

/*!
    \brief Intersection utilities.

    Utility functions for ray-triangle intersections.
*/
class IntersectionUtils
{
public:

    LM_DISABLE_CONSTRUCT(IntersectionUtils);

public:

    /*!
        \brief Create intersection structure.

        Helper function to fill in the intersection data from the triangle intersection info.
        This function is utilized in the implementation of Accel interface.

        \param primitive Primitive on the intersection point.
        \param p Intersection point
        \param b Barycentric coordinates of the triangle at the intersection point.
        \param faceIndex Triangle face index.
    */
    static auto CreateTriangleIntersection(const Primitive* primitive, const Vec3& p, const Vec2& b, int faceIndex) -> Intersection
    {
        Intersection isect;

        // Store information
        const auto* mesh = primitive->mesh;
        isect.primitive = primitive;

        // Intersection point
        isect.geom.p = p;

        // Face indices
        const auto* fs = mesh->Faces();
        int v1 = fs[3 * faceIndex];
        int v2 = fs[3 * faceIndex + 1];
        int v3 = fs[3 * faceIndex + 2];

        // Geometry normal
        const auto* ps = mesh->Positions();
        Vec3 p1(primitive->transform * Vec4(ps[3 * v1], ps[3 * v1 + 1], ps[3 * v1 + 2], 1_f));
        Vec3 p2(primitive->transform * Vec4(ps[3 * v2], ps[3 * v2 + 1], ps[3 * v2 + 2], 1_f));
        Vec3 p3(primitive->transform * Vec4(ps[3 * v3], ps[3 * v3 + 1], ps[3 * v3 + 2], 1_f));
        isect.geom.gn = Math::Normalize(Math::Cross(p2 - p1, p3 - p1));

        // Shading normal
        Vec3 n1, n2, n3;
        const auto* ns = mesh->Normals();
        if (ns)
        {
            n1 = primitive->normalTransform * Vec3(ns[3 * v1], ns[3 * v1 + 1], ns[3 * v1 + 2]);
            n2 = primitive->normalTransform * Vec3(ns[3 * v2], ns[3 * v2 + 1], ns[3 * v2 + 2]);
            n3 = primitive->normalTransform * Vec3(ns[3 * v3], ns[3 * v3 + 1], ns[3 * v3 + 2]);
            isect.geom.sn = Math::Normalize(n1 * (1_f - b[0] - b[1]) + n2 * b[0] + n3 * b[1]);
            if (std::isnan(isect.geom.sn.x) || std::isnan(isect.geom.sn.y) || std::isnan(isect.geom.sn.z))
            {
                // There is a case with one of n1 ~ n3 generates NaN
                // possibly a bug of mesh loader?
                isect.geom.sn = isect.geom.gn;
            }
        }
        else
        {
            // Use geometric normal
            isect.geom.sn = n1 = n2 = n3 = isect.geom.gn;
        }

        // Texture coordinates
        const auto* tc = mesh->Texcoords();
        if (tc)
        {
            Vec2 uv1(tc[2 * v1], tc[2 * v1 + 1]);
            Vec2 uv2(tc[2 * v2], tc[2 * v2 + 1]);
            Vec2 uv3(tc[2 * v3], tc[2 * v3 + 1]);
            isect.geom.uv = uv1 * (1_f - b[0] - b[1]) + uv2 * b[0] + uv3 * b[1];
        }

        // Scene surface is not degenerated
        isect.geom.degenerated = false;

        // Compute tangent space
        Math::OrthonormalBasis(isect.geom.sn, isect.geom.dpdu, isect.geom.dpdv);
        isect.geom.ToWorld = Mat3(isect.geom.dpdu, isect.geom.dpdv, isect.geom.sn);
        isect.geom.ToLocal = Math::Transpose(isect.geom.ToWorld);

        // Compute normal derivative
        const auto N = n1 * (1_f - b[0] - b[1]) + n2 * b[0] + n3 * b[1];
        const auto invNLen = 1_f / Math::Length(N);
        const auto dNdu = (n2 - n1) * invNLen;
        const auto dNdv = (n3 - n2) * invNLen;
        isect.geom.dndu = dNdu - isect.geom.sn * Math::Dot(dNdu, isect.geom.sn);
        isect.geom.dndv = dNdv - isect.geom.sn * Math::Dot(dNdv, isect.geom.sn);

        return isect;
    }

};

LM_NAMESPACE_END
