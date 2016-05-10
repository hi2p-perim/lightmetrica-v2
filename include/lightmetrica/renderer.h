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

LM_NAMESPACE_BEGIN

class Scene;
class Film;

/*!
    \defgroup renderer Renderer
    \brief Abstraction of renderers
*/

/*!
    \brief A base class of the renderer.
    \ingroup renderer
*/
class Renderer : public Configurable
{
public:

    LM_INTERFACE_CLASS(Renderer, Configurable, 1);

public:

    Renderer() = default;
    LM_DISABLE_COPY_AND_MOVE(Renderer);

public:

    /*!
        \brief Render an image.
        The function starts to render the `scene` to the `film`.
        \retval true Succeeded to render the scene.
        \retval true Failed to render the scene.
    */
    LM_INTERFACE_F(0, Render, void(const Scene*, Film*));

};

LM_NAMESPACE_END

