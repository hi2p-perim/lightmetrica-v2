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

#include <lightmetrica/configurable.h>
#include <lightmetrica/math.h>

LM_NAMESPACE_BEGIN

class Scene;
struct Primitive;
struct Ray;
struct Intersection;

class Accel : public Configurable
{
public:

    LM_INTERFACE_CLASS(Accel, Configurable);

public:

    Accel() = default;
    LM_DISABLE_COPY_AND_MOVE(Accel);

public:

    LM_INTERFACE_F(Build, bool(const Scene& scene));
    LM_INTERFACE_F(Intersect, bool(const Scene& scene, const Ray& ray, Intersection& isect, Float minT, Float maxT));

};

LM_NAMESPACE_END
