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

#include "inversemaputils.h"

#if LM_DEBUG_MODE
#define INVERSEMAP_MANIFOLDWALK_DEBUG_IO 0
#else
#define INVERSEMAP_MANIFOLDWALK_DEBUG_IO 0
#endif

LM_NAMESPACE_BEGIN

struct VertexConstraintJacobian
{
    Mat2 A;
    Mat2 B;
    Mat2 C;
};

using ConstraintJacobian = std::vector<VertexConstraintJacobian>;

class ManifoldUtils
{
public:

    LM_DISABLE_CONSTRUCT(ManifoldUtils);

public:

    static auto ComputeConstraintJacobian(const Subpath& path, ConstraintJacobian& nablaC) -> void;
    static auto WalkManifold(const Scene* scene, const Subpath& seedPath, const Vec3& target)->boost::optional<Subpath>;
    static auto WalkManifold(const Scene* scene, const Subpath& seedPath, const Vec3& target, Subpath& connPath) -> bool;
    static auto ComputeConstraintJacobianDeterminant(const Subpath& subpath) -> Float;

};

LM_NAMESPACE_END
