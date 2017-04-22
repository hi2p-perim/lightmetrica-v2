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
#include <lightmetrica/spectrum.h>

LM_NAMESPACE_BEGIN

class Assets;
class PropertyNode;
struct SurfaceGeometry;
struct Primitive;
class Scene;

/*!
    \brief Base class of an asset.

    The base class of the asset classes.
    The `asset` is an important concept in the framework.
    All user-defined resources such as triangle meshes or BSDFs must inherits this class.
    The construction of assets are fully automated with asset management class (`Assets` class),
    which make it possible to extend your own assets consistently.

    \ingroup asset
*/
class Asset : public BasicComponent
{
public:

    LM_INTERFACE_CLASS(Asset, BasicComponent, 2);

public:

    Asset() = default;
    LM_DISABLE_COPY_AND_MOVE(Asset);

public:

    /*!
        \brief Load an asset from a property node.

        Configure and initialize the asset by a property node given by `prop`.
        Some assets have references to the other assets, so `assets` is also required.
        Dependent asset must be loaded beforehand.

        Also, some asset requires primitive information (e.g., transformation or meshes).
        In this case you can use information obtained from `primitive`.
        
        The property node contains the tree structure below the `params` node
        in the asset definitions. For instance, the asset defined by the following 
        configuration creates a property with two nodes 'A' and 'B'.
        The values can be accessed from the interfaces of PropertyNode class.

            some_asset:
                interface: some_interface
                type: some_type
                params:
                    A: some_value_1
                    B: some_value_2

        \param prop Property node for the asset.
        \param assets Asset manager.
        \retval true Succeeded to load.
        \retval false Failed to load.
	*/
    LM_INTERFACE_F(0, Load, bool(const PropertyNode* prop, Assets* assets, const Primitive* primitive));

    /*!
        \brief Post processing of the asset.

        The function is called when the all primitives are loaded.
        If the asset needs postprocessing that requires scene information
        (e.g., scene bound, etc.), we can implement this function.

        \param scene Scene.
        \retval true Succeeded to load.
        \retval false Failed to load.
    */
    LM_INTERFACE_F(1, PostLoad, bool(const Scene* scene));

public:

    //! \cond detail
    auto ID() const -> std::string { return id_.Get(); }
    auto SetID(const std::string& id) -> void { id_.Set(id); }
    auto Index() const -> int { return index_; }
    auto SetIndex(int index) -> void { index_ = index; }
    //! \endcond

private:

    Portable<std::string> id_;
    int index_;

};

LM_NAMESPACE_END
