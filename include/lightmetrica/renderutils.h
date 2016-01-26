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

LM_NAMESPACE_BEGIN

class RenderUtils
{
public:

    LM_DISABLE_CONSTRUCT(RenderUtils);

public:

    auto GeometryTerm(const SurfaceGeometry& geom1, const SurfaceGeometry& geom2) -> Float
    {
        auto p1p2 = geom2.p - geom1.p;
        const auto p1p2L2 = Math::Dot(p1p2, p1p2);
        const auto p1p2L = Math::Sqrt(p1p2L2);
        p1p2 /= p1p2L;
        Float t = 1_f;
        if (!geom1.degenerated) { t *= Math::Abs(Math::Dot(geom1.sn, p1p2)); }
        if (!geom2.degenerated) { t *= Math::Abs(Math::Dot(geom2.sn, -p1p2)); }
        return t / p1p2L2;
    }

};

LM_NAMESPACE_END
