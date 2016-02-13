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
#include <lightmetrica/ray.h>

LM_NAMESPACE_BEGIN

/*!
    \addtogroup math
    \{
*/

//! Axis-aligned bounding box.
struct Bound
{

    Vec3 min = Vec3( Math::Inf());
    Vec3 max = Vec3(-Math::Inf());

    auto operator[](int i) const -> const Vec3& { return (&min)[i]; }
    auto operator[](int i)       -> Vec3&       { return (&max)[i]; }

    auto LongestAxis() const -> int
    {
        const auto d = max - min;
        return d.x > d.y && d.x > d.z ? 0 : d.y > d.z ? 1 : 2;
    }

    auto SurfaceArea() const -> Float
    {
        const auto d = max - min;
        return 2_f * (d.x * d.y + d.y * d.z + d.z * d.x);
    }

    auto Centroid() const -> Vec3
    {
        return (min + max) * 0.5_f;
    }

};

namespace Math
{
    //! Merge two bounds
    LM_INLINE auto Union(const Bound& a, const Bound& b) -> Bound
    {
        Bound r;
        r.min = Math::Min(a.min, b.min);
        r.max = Math::Max(a.max, b.max);
        return r;
    }

    //! Merge one bound and a point
    LM_INLINE auto Union(const Bound& a, const Vec3& p) -> Bound
    {
        Bound r;
        r.min = Math::Min(a.min, p);
        r.max = Math::Max(a.max, p);
        return r;
    }

    //! Intersection query between ray and bound
    LM_INLINE auto IntersectBound(const Bound& bound, const Ray& ray, Float tMin, Float tMax) -> bool
    {
        const bool rayDirNeg[3] = { ray.d.x < 0_f, ray.d.y < 0_f, ray.d.z < 0_f };
        Float tmin;
        Float tmax;

        const Float txmax = (bound[1 - rayDirNeg[0]].x - ray.o.x) * (ray.d.x == 0 ? Math::Inf() : 1.0_f / ray.d.x);
        const Float txmin = (bound[rayDirNeg[0]].x - ray.o.x) * (ray.d.x == 0 ? 0_f : 1.0_f / ray.d.x);
        tmin = txmin;
        tmax = txmax;

        const Float tymax = (bound[1 - rayDirNeg[1]].y - ray.o.y) * (ray.d.y == 0 ? Math::Inf() : 1.0_f / ray.d.y);
        const Float tymin = (bound[rayDirNeg[1]].y - ray.o.y) * (ray.d.y == 0 ? 0_f : 1.0_f / ray.d.y);
        if ((tmin > tymax) || (tymin > tmax)) return false;
        if (tymin > tmin) tmin = tymin;
        if (tymax < tmax) tmax = tymax;

        const Float tzmax = (bound[1 - rayDirNeg[2]].z - ray.o.z) * (ray.d.z == 0 ? Math::Inf() : 1.0_f / ray.d.z);
        const Float tzmin = (bound[rayDirNeg[2]].z - ray.o.z) * (ray.d.z == 0 ? 0_f : 1.0_f / ray.d.z);
        if ((tmin > tzmax) || (tzmin > tmax)) return false;
        if (tzmin > tmin) tmin = tzmin;
        if (tzmax < tmax) tmax = tzmax;

        return (tmin < tMax) && (tmax > tMin);
    }
}

//! \}

LM_NAMESPACE_END