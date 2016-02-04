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
class Scene;

/*!
    \defgroup asset Asset
    \brief Asset system of the framework.
*/

/*!
    \brief Asset library.

    An interface for the asset management.
    All instances of the loaded assets are managed by this interface.

    \ingroup asset
*/
class Assets : public Component
{
public:

    LM_INTERFACE_CLASS(Assets, Component, 3);

public:

    Assets() = default;
    LM_DISABLE_COPY_AND_MOVE(Assets);

public:

    /*!
        \brief Initialize the asset library.

        Initializes the asset management.
        Note that this function does *not* load any assets.
        The asset loading is delayed until they are actually referenced.

        \params prop The propery node pointing to `assets` node.
        \retval true Succeeded to initialize.
        \retval false Failed to initialize.
    */
    LM_INTERFACE_F(0, Initialize, bool(const PropertyNode* prop));

    /*!
        \brief Get and/or load an asset by name.

        The function tries to get the asset specified by `id`.
        If the asset is not found, the asset is loaded from the property node
        specified by the Initialize function.

        This function returns `nullptr` if no asset is found,
        or failed to load the asset.

        \param name Name of the asset.
        \return Asset instance.
    */
    LM_INTERFACE_F(1, AssetByIDAndType, Asset*(const std::string& id, const std::string& type, const Primitive* primitive));

    /*!
        \brief Dispatches post loading functions of the loaded assets.
        \param scene Scene.
        \retval true Succeeded to load.
        \retval false Failed to load.
    */
    LM_INTERFACE_F(2, PostLoad, bool(const Scene* scene));

};

LM_NAMESPACE_END