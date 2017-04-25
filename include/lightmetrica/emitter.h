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

#include <lightmetrica/surfaceinteraction.h>

LM_NAMESPACE_BEGIN

class PositionSampler;
struct Ray;
struct Intersection;
struct Bound;

/*!
    \brief Shape associated with Emitter.
    \ingroup asset
*/
class EmitterShape : public Component
{
public:

    LM_INTERFACE_CLASS(EmitterShape, Component, 2);

public:

    EmitterShape() = default;
    LM_DISABLE_COPY_AND_MOVE(EmitterShape);

public:

    //! Intersect query with the shape.
    LM_INTERFACE_F(0, Intersect, bool(const Ray& ray, Float minT, Float maxT, Intersection& isect));
    
    //! Get primitive associated to the shape
    //LM_INTERFACE_F(1, GetPrimitive, const Primitive*());

};

/*!
    \brief An interface for Emitter.
    \ingroup asset
*/
class Emitter : public SurfaceInteraction
{
public:

    LM_INTERFACE_CLASS(Emitter, SurfaceInteraction, 2);

public:

    Emitter() = default;
    LM_DISABLE_COPY_AND_MOVE(Emitter);

public:

    /*!
        \brief Get emitter shape.
    
        Some emitter is associated with shapes that can be intersected
        (e.g., sphere for environment lights) in order to integrate emitters
        with BPT based rendering techniques.

        \return Instance of emitter shape.
    */
    LM_INTERFACE_F(0, GetEmitterShape, const EmitterShape*());

    /*!
        \brief Get bound of the emitter.
        \return Bound.
    */
    LM_INTERFACE_F(1, GetBound, Bound());

};

LM_NAMESPACE_END
