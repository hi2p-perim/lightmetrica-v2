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

#include <lightmetrica/lightmetrica.h>

LM_NAMESPACE_BEGIN

struct TextureOp : public Component
{
    LM_INTERFACE_CLASS(TextureOp, Component, 1);
    LM_INTERFACE_F(0, Apply, Vec3(const Vec3& c1, const Vec3& c2));
};

struct TextureOp_C1 : public TextureOp
{
    LM_IMPL_CLASS(TextureOp_C1, TextureOp);
    LM_IMPL_F(Apply) = [this](const Vec3& c1, const Vec3& c2) { return c1; };
};

struct TextureOp_C2 : public TextureOp
{
    LM_IMPL_CLASS(TextureOp_C2, TextureOp);
    LM_IMPL_F(Apply) = [this](const Vec3& c1, const Vec3& c2) { return c2; };
};

struct TextureOp_Max : public TextureOp
{
    LM_IMPL_CLASS(TextureOp_Max, TextureOp);
    LM_IMPL_F(Apply) = [this](const Vec3& c1, const Vec3& c2) { return Math::Max(c1, c2); };
};

struct TextureOp_Min : public TextureOp
{
    LM_IMPL_CLASS(TextureOp_Min, TextureOp);
    LM_IMPL_F(Apply) = [this](const Vec3& c1, const Vec3& c2) { return Math::Min(c1, c2); };
};

LM_COMPONENT_REGISTER_IMPL(TextureOp_C1, "textureop::c1");
LM_COMPONENT_REGISTER_IMPL(TextureOp_C2, "textureop::c2");
LM_COMPONENT_REGISTER_IMPL(TextureOp_Max, "textureop::max");
LM_COMPONENT_REGISTER_IMPL(TextureOp_Min, "textureop::min");

// --------------------------------------------------------------------------------

class Texture_BinaryOp final : public Texture
{
public:

    LM_IMPL_CLASS(Texture_BinaryOp, Texture);

public:

    LM_IMPL_F(Load) = [this](const PropertyNode* prop, Assets* assets, const Primitive* primitive) -> bool
    {
        tex1_ = static_cast<const Texture*>(assets->AssetByIDAndType(prop->Child("tex1")->As<std::string>(), "texture", primitive));
        tex2_ = static_cast<const Texture*>(assets->AssetByIDAndType(prop->Child("tex2")->As<std::string>(), "texture", primitive));
        op_ = ComponentFactory::Create<TextureOp>("textureop::" + prop->Child("op")->As<std::string>());
        return true;
    };

    LM_IMPL_F(Evaluate) = [this](const Vec2& uv) -> Vec3
    {
        return op_->Apply(
            tex1_->Evaluate(uv),
            tex2_->Evaluate(uv));
    };

private:

    const Texture* tex1_;
    const Texture* tex2_;
    TextureOp::UniquePtr op_{ nullptr, nullptr };

};

LM_COMPONENT_REGISTER_IMPL(Texture_BinaryOp, "texture::binary_op")

LM_NAMESPACE_END
