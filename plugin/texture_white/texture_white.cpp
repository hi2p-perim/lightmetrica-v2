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

#include <lightmetrica/texture.h>

LM_NAMESPACE_BEGIN

class Texture_White final : public Texture
{
public:

    LM_IMPL_CLASS(Texture_White, Texture);

public:

    LM_IMPL_F(Load) = [this](const PropertyNode* prop, Assets* assets, const Primitive* primitive) -> bool
    {
        return true;
    };

    LM_IMPL_F(PostLoad) = [this](const Scene* scene) -> bool
    {
        return true;
    };

    LM_IMPL_F(Evaluate) = [this](const Vec2& uv) -> Vec3
    {
        // Always returns (1,1,1)
        return Vec3(1_f);
    };
};

LM_COMPONENT_REGISTER_IMPL(Texture_White, "texture::white")

LM_NAMESPACE_END