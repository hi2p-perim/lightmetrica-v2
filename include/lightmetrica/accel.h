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

/*!
    \defgroup accel Accel
    \brief Acceleration structure for ray-triangle intersection.
*/

/*!
    \brief An interface for the acceleration structure.
    \ingroup accel
*/
class Accel : public Configurable
{
public:

    LM_INTERFACE_CLASS(Accel, Configurable, 2);

public:

    Accel() = default;
    LM_DISABLE_COPY_AND_MOVE(Accel);

public:

    /*!
        \brief Build the acceleration structure.

        Some scene may have an acceleration structure for the optimization.
        The function is automatically called by the Asset::Load function.

        \param scene  Loaded scene.
        \retval true  Succeeded to build.
        \retval false Failed to build.
    */
    LM_INTERFACE_F(0, Build, bool(const Scene* scene));

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
    LM_INTERFACE_F(1, Intersect, bool(const Scene* scene, const Ray& ray, Intersection& isect, Float minT, Float maxT));

};

LM_NAMESPACE_END
