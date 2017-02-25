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

#include <lightmetrica/accel.h>

LM_NAMESPACE_BEGIN

struct Primitive;
struct Ray;
struct Intersection;

/*!
    \brief An interface for the acceleration structure for 3-dimensional scenes.
    \ingroup accel
*/
class Accel3 : public Accel
{
public:

    LM_INTERFACE_CLASS(Accel3, Accel, 1);

public:

    Accel3() = default;
    LM_DISABLE_COPY_AND_MOVE(Accel3);

public:

    /*!
        \brief Intersection query with triangles.

        The function checks if `ray` hits with the scene.
        This function is supposed to be accelerated by spatial acceleration structure.
        When intersected, information on the hit point is stored in the intersection data.
        The intersection is valid only with the range of the distance between `minT` and `maxT`.

        \param scene  Scene.
        \param ray    Ray.
        \param isect  Intersection data.
        \param minT   Minimum range of the distance.
        \param minT   Maximum range of the distance.
        \retval true  Intersected with the scene.
        \retval false Not intersected with the scene.
    */
    LM_INTERFACE_F(0, Intersect, bool(const Scene* scene, const Ray& ray, Intersection& isect, Float minT, Float maxT));

};

LM_NAMESPACE_END
