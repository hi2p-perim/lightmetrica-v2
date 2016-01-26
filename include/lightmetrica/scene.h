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

LM_NAMESPACE_BEGIN

class PropertyNode;
class Assets;
class Accel;
struct Primitive;
struct Ray;
struct Intersection;

/*!
    \brief Scene.

    A base class of the scene.
*/
class Scene : public Component
{
public:

    LM_INTERFACE_CLASS(Scene, Component);

public:

    Scene() = default;
    LM_DISABLE_COPY_AND_MOVE(Scene);

public:

    /*!
        \brief Initialize the scene.

        Initializes the scene from the given property of
        the scene configuration file.
    */
    LM_INTERFACE_F(Initialize, bool(const PropertyNode*, Assets*, Accel*));

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
    LM_INTERFACE_F(Intersect, bool(const Ray& ray, Intersection&));

    /*!
        \brief Get a primitive by ID.

        \param id ID of a primitive.
        \return Primitive.
    */
    LM_INTERFACE_F(PrimitiveByID, const Primitive*(const std::string&));

    /*!
        \brief Get the number of primitives.
    */
    LM_INTERFACE_F(NumPrimitives, int());

    /*!
        \brief Get a primitive by index.
        \param index Index of a primitive.
    */
    LM_INTERFACE_F(PrimitiveAt, const Primitive*(int index));

    /*!
        \brief Get a sensor primitive.
    */
    LM_INTERFACE_F(Sensor, const Primitive*());

    LM_INTERFACE_F(SampleEmitter, const Primitive*(int type, Float u));
    LM_INTERFACE_F(EvaluateEmitterPDF, Float(int type));

};

LM_NAMESPACE_END
