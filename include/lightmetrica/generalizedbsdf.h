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

#include <lightmetrica/asset.h>
#include <lightmetrica/math.h>

LM_NAMESPACE_BEGIN

struct SurfaceGeometry;

/*!
*/
namespace SurfaceInteraction
{
    enum Type
    {
        D = 1 << 0,
        G = 1 << 1,
        S = 1 << 2,
        L = 1 << 3,
        E = 1 << 4,
        BSDF = D | G | S,
        Emitter = L | E,
        None = 0
    };
};

/*!
*/
enum class TransportDirection
{
    LE,
    EL
};

/*!
*/
class GeneralizedBSDF : public Asset
{
public:

    LM_INTERFACE_CLASS(GeneralizedBSDF, Asset);

public:

    GeneralizedBSDF() = default;
    LM_DISABLE_COPY_AND_MOVE(GeneralizedBSDF);

public:

    LM_INTERFACE_F(EvaluateDirection, bool(const SurfaceGeometry&, int types, const Vec3& wi, const Vec3& wo, TransportDirection transDir, bool evalDelta));

};

LM_NAMESPACE_END
