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

#include <pch.h>
#include <lightmetrica/renderer.h>

LM_NAMESPACE_BEGIN

/*!
    \brief Vertex connection and merging renderer (reference version).

    Implements vertex conneection and merging [Georgiev et al. 2012].
    This implementation purposely adopts a naive way
    to check the correctness of the implementation and
    to be utilized as a baseline for the further modifications.

    For the optimized implementation, see `renderer::vcm`,
    which is based on the way described in the technical report [Georgiev 2012]
    or SmallVCM renderer [Davidovic & Georgiev 2012].

    References:
      - [Georgiev et al. 2012] Light transport simulation with vertex connection and merging
      - [Hachisuka et al. 2012] A path space extension for robust light transport simulation
      - [Georgiev 2012] Implementing vertex connection and merging
      - [Davidovic & Georgiev 2012] SmallVCM renderer 
*/
class Renderer_VCM_Reference final : public Renderer
{
public:

    LM_IMPL_CLASS(Renderer_VCM_Reference, Renderer);

public:

    LM_IMPL_F(Initialize) = [this](const PropertyNode* prop) -> bool
    {
        return true;
    };

    LM_IMPL_F(Render) = [this](const Scene* scene, Film* film) -> void
    {
        
    };

};

LM_COMPONENT_REGISTER_IMPL(Renderer_VCM_Reference, "renderer::vcmref");

LM_NAMESPACE_END
