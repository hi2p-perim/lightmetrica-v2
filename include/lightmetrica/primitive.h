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

#include <lightmetrica/portable.h>
#include <lightmetrica/math.h>
#include <lightmetrica/bsdf.h>
#include <lightmetrica/emitter.h>
#include <lightmetrica/align.h>
#include <lightmetrica/bound.h>
#include <string>
#include <cassert>

LM_NAMESPACE_BEGIN

class TriangleMesh;
class BSDF;
class Emitter;
class Light;
class Sensor;

/*!
	\brief Primitive.

	Primitive is an element of the scene used for managing transformable objects.
	A primitive corresponds to a node in the scene.

    TODO: Redesign sampling related functions.

    \ingroup scene
*/
struct Primitive : public SIMDAlignedType
{

    // Primitive ID
    const char* id = nullptr;

    // Transform & normal transform
    Mat4 transform;
    Mat3 normalTransform;

    // Triangle mesh
    const TriangleMesh* mesh = nullptr;

    // Surface interactions
    const SurfaceInteraction* surface = nullptr;
    const BSDF*    bsdf               = nullptr;
    const Emitter* emitter            = nullptr;
    const Light*   light              = nullptr;
    const Sensor*  sensor             = nullptr;

    /*!
        Get underlying surface interaction types.
        \return Surface interaction types.
    */
    //auto Type() const -> int
    //{
    //    int type = 0;
    //    if (emitter)
    //    {
    //        type |= emitter->Type();
    //    }
    //    if (bsdf)
    //    {
    //        type |= bsdf->Type();
    //    }
    //    return type;
    //}

    /*!
        Select the underlying surface interaction as given type.
        \param queryType Surface interaction type.
        \return          Selected surface interaction.
    */
    //auto As(int queryType) const -> const SurfaceInteraction*
    //{
    //    assert((queryType & SurfaceInteractionType::Emitter) == 0 || (queryType & SurfaceInteractionType::BSDF) == 0);
    //    if ((queryType & SurfaceInteractionType::Emitter) > 0)
    //    {
    //        return emitter;
    //    }
    //    if ((queryType & SurfaceInteractionType::BSDF) > 0)
    //    {
    //        return bsdf;
    //    }
    //    LM_UNREACHABLE();
    //    return nullptr;
    //}

};

LM_NAMESPACE_END
