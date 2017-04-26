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

/*!
    \defgroup scene Scene
    \brief Scene configuration of the framework.
*/

/*!
    \brief A base class of the scene.
    \ingroup scene
*/
class Scene : public BasicComponent
{
public:

    LM_INTERFACE_CLASS(Scene, BasicComponent, 3);

public:

    /*!
        \brief Initialize the scene.

        Initializes the scene from the given property of
        the scene configuration file.
    */
    LM_INTERFACE_F(0, Initialize, bool(const PropertyNode* sceneNode, Assets* assets, Accel* accel));

    //! Gets the instance of the asset manager.
    LM_INTERFACE_F(1, GetAssets, const Assets*());

    //! Gets the instance of the acceleration structure.
    LM_INTERFACE_F(2, GetAccel, const Accel*());

};

LM_NAMESPACE_END
