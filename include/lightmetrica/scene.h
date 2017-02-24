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

#include <lightmetrica/component.h>
#include <lightmetrica/math.h>
#include <lightmetrica/ray.h>
#include <lightmetrica/intersection.h>
#include <lightmetrica/bound.h>
#include <lightmetrica/probability.h>

LM_NAMESPACE_BEGIN

class PropertyNode;
class Assets;
class Accel;
struct Primitive;
struct Ray;
struct Intersection;

/*!
    \defgroup scene Scene
    \brief Scene configuration of the framework.
*/

class Scene : public Component
{
public:

    LM_INTERFACE_CLASS(Scene, Component, 1);

public:

    /*!
        \brief Initialize the scene.

        Initializes the scene from the given property of
        the scene configuration file.
    */
    LM_INTERFACE_F(0, Initialize, bool(const PropertyNode*, Assets*, Accel*));

};

/*!
    \brief A base class of the scene.
    \ingroup scene
*/
class Scene3 : public Scene
{
public:

    LM_INTERFACE_CLASS(Scene3, Scene, 10);

public:

    Scene3() = default;
    LM_DISABLE_COPY_AND_MOVE(Scene3);

public:

    /*!
        \brief Intersection query.

        The function checks if `ray` hits with the scene.
		Returns `true` if the ray intersected, otherwise returns `false`.
        The information on the hit point is stored in the intersection data.

        \param ray Ray.
        \param isect Intersection data.
        \retval true Intersected with the scene.
        \retval false Not intersected with the scene.
    */
    LM_INTERFACE_F(0, Intersect, bool(const Ray& ray, Intersection&));
    LM_INTERFACE_F(1, IntersectWithRange, bool(const Ray& ray, Intersection& isect, Float minT, Float maxT));

    /*!
        \brief Get a primitive by ID.

        \param id ID of a primitive.
        \return Primitive.
    */
    LM_INTERFACE_F(2, PrimitiveByID, const Primitive*(const std::string&));

    /*!
        \brief Get the number of primitives.
    */
    LM_INTERFACE_F(3, NumPrimitives, int());

    /*!
        \brief Get a primitive by index.
        \param index Index of a primitive.
    */
    LM_INTERFACE_F(4, PrimitiveAt, const Primitive*(int index));

    /*!
        \brief Get a sensor primitive.
    */
    LM_INTERFACE_F(5, GetSensor, const Primitive*());

    LM_INTERFACE_F(6, SampleEmitter, const Primitive*(int type, Float u));
    LM_INTERFACE_F(7, EvaluateEmitterPDF, PDFVal(const Primitive* primitive));

    //! Compute the bound of the scene
    LM_INTERFACE_F(8,  GetBound, Bound());
    LM_INTERFACE_F(9, GetSphereBound, SphereBound());

public:

    auto Visible(const Vec3& p1, const Vec3& p2) const -> bool
    {
        Ray shadowRay;
        const auto p1p2 = p2 - p1;
        const auto p1p2L = Math::Length(p1p2);
        shadowRay.d = p1p2 / p1p2L;
        shadowRay.o = p1;
        Intersection _;
        return !IntersectWithRange(shadowRay, _, Math::EpsIsect(), p1p2L * (1_f - Math::EpsIsect()));
    }

};

LM_NAMESPACE_END
