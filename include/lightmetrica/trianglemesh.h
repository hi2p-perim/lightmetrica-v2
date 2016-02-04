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
#include <lightmetrica/bound.h>

LM_NAMESPACE_BEGIN

/*!
    \defgroup triangle_mesh Triangle mesh
    \brief Triangle meshes.
    \ingroup asset
*/

/*!
	\brief An interface for the triangle meshes.
    \ingroup triangle_mesh
*/
class TriangleMesh : public Asset
{
public:

    LM_INTERFACE_CLASS(TriangleMesh, Asset, 6);

public:

    TriangleMesh() = default;
    LM_DISABLE_COPY_AND_MOVE(TriangleMesh);

public:

    /*!
        Get the number of vertices.
        \return The number of vertices.
    */
    LM_INTERFACE_F(0, NumVertices, int());

    /*!
        Get the number of faces.
        \return The number of faces.
    */
    LM_INTERFACE_F(1, NumFaces, int());

    /*!
        Get the position array.
        \return The position array.
    */
    LM_INTERFACE_F(2, Positions, const Float*());

    /*!
        Get the normal array.
        \return The normal array.
    */
    LM_INTERFACE_F(3, Normals, const Float*());

    /*!
        Get the texture coordinates array.
        \return The texture coordinates array.
    */
    LM_INTERFACE_F(4, Texcoords, const Float*());

    /*!
        Get the face array.
        \return The face array.
    */
    LM_INTERFACE_F(5, Faces, const unsigned int*());

};

LM_NAMESPACE_END
