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

LM_NAMESPACE_BEGIN

/*!
    \brief Surface geometry information.

    The surface geometry information of the intersected point
    is stored in this structure.

    \ingroup core
*/
struct SurfaceGeometry
{

    bool degenerated;   //!< True if the point is spatially degenerated, e.g., point light source
    Vec3 p;             //!< Intersection point
    Vec3 sn;            //!< Shading normal
    Vec3 gn;            //!< Geometry normal
    Vec3 dpdu, dpdv;    //!< Tangent vectors
    Vec3 dndu, dndv;    //!< Partial derivatives of shading normal
    Vec2 uv;            //!< Texture coordinates
    Mat3 ToLocal;       //!< Conversion matrix from world coordinates to shading coordinates 
    Mat3 ToWorld;       //!< Conversion matrix from shading coordinates to world coordinates 

};

LM_NAMESPACE_END
