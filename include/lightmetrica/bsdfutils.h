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

#include <lightmetrica/macros.h>
#include <lightmetrica/math.h>
#include <lightmetrica/surfacegeometry.h>
#include <lightmetrica/surfaceinteraction.h>

LM_NAMESPACE_BEGIN

class BSDFUtils
{
public:

    LM_DISABLE_CONSTRUCT(BSDFUtils);

public:

    static auto ShadingNormalCorrection(const SurfaceGeometry& geom, const Vec3& wi, const Vec3& wo, TransportDirection transDir) -> Float
    {
        const auto localWi = geom.ToLocal * wi;
        const auto localWo = geom.ToLocal * wo;
        const Float wiDotNg = Math::Dot(wi, geom.gn);
        const Float woDotNg = Math::Dot(wo, geom.gn);
        const Float wiDotNs = Math::LocalCos(localWi);
        const Float woDotNs = Math::LocalCos(localWo);
        if (wiDotNg * wiDotNs <= 0_f || woDotNg * woDotNs <= 0_f) { return 0_f; }
        if (transDir == TransportDirection::LE) { return wiDotNs * woDotNg / (woDotNs * wiDotNg); }
        return 1_f;
    }

    static auto LocalReflect(const Vec3& wi) -> Vec3
    {
        return Vec3(-wi.x, -wi.y, wi.z);
    }

    static auto LocalRefract(const Vec3& wi, Float eta, Float cosThetaT) -> Vec3
    {
        return Vec3(-eta * wi.x, -eta * wi.y, cosThetaT);
    }

};

LM_NAMESPACE_END
