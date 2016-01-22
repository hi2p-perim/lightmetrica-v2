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

LM_NAMESPACE_BEGIN

class Asset;
class PropertyNode;
struct Primitive;

/*!
    Asset library.

    Manages all instances of the assets.
*/
class Assets : public Component
{
public:

    LM_INTERFACE_CLASS(Assets, Component);

public:

    Assets() = default;
    LM_DISABLE_COPY_AND_MOVE(Assets);

public:

    LM_INTERFACE_F(Initialize, bool(const PropertyNode*));
    LM_INTERFACE_F(AssetByIDAndType, const Asset*(const std::string& id, const std::string& type, const Primitive* primitive));

public:

    template <typename AssetType>
    auto AssetByID(const std::string& id, const Primitive* primitive = nullptr) const -> const AssetType*
    {
        static_assert(std::is_base_of<Asset, AssetType>::value, "Asset must be a base of AssetType");
        const auto* asset = AssetByIDAndType(id, AssetType::Type_().name, primitive);
        return static_cast<const AssetType*>(asset);
    }

};

LM_NAMESPACE_END
