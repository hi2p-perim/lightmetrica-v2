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
#include <lightmetrica/property.h>

LM_NAMESPACE_BEGIN

class Texture_Checker final : public Texture
{
public:

    LM_IMPL_CLASS(Texture_Checker, Texture);

public:

    LM_IMPL_F(Load) = [this](const PropertyNode* prop, Assets* assets, const Primitive* primitive) -> bool
    {
        scale_ = prop->ChildAs<Float>("scale", 100_f);
        color1_ = prop->ChildAs<Vec3>("color1", Vec3(1_f, 0_f, 0_f));
        color2_ = prop->ChildAs<Vec3>("color2", Vec3(1_f));
        return true;
    };

    LM_IMPL_F(Evaluate) = [this](const Vec2& uv) -> Vec3
    {
        const int u = (int)(uv.x * scale_);
        const int v = (int)(uv.y * scale_);
        return (u + v) % 2 == 0 ? color1_ : color2_;
    };

private:

    Float scale_;
    Vec3 color1_;
    Vec3 color2_;

};

LM_COMPONENT_REGISTER_IMPL(Texture_Checker, "texture::checker")

LM_NAMESPACE_END