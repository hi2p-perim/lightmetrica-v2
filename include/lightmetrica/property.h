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
#include <sstream>
#include <vector>

LM_NAMESPACE_BEGIN

class PropertyTree;

enum class PropertyNodeType : int
{
    Null,
    Scalar,
    Sequence,
    Map,
    Undefined,
};

/*!
    Property node.

    An element of the property.
*/
class PropertyNode : public Component
{
public:

    LM_INTERFACE_CLASS(PropertyNode, Component);

public:

    PropertyNode() = default;
    LM_DISABLE_COPY_AND_MOVE(PropertyNode);

public:

    LM_INTERFACE_F(Tree, const PropertyTree*());
    LM_INTERFACE_F(Type, PropertyNodeType());
    LM_INTERFACE_F(Line, int());

    /*!
        Key of the node.
        Only available for `Map` type.
    */
    LM_INTERFACE_F(Key, std::string());

    /*!
        Scalar value of the node.
        Only available for `Scalar` type.
    */
    LM_INTERFACE_F(Scalar, std::string());
    LM_INTERFACE_F(RawScalar, const char*());

    /*!
        Get a number of child elements.
        Only available for `Sequence` type.
    */
    LM_INTERFACE_F(Size, int());

    /*!
        Find a child by name.
        Only available for `Map` type.
    */
    LM_INTERFACE_F(Child, const PropertyNode*(const std::string&));

    /*!
        Get a child by index.
        Only available for `Sequence` type.
    */
    LM_INTERFACE_F(At, const PropertyNode*(int));

    /*!
        Parent node (nullptr for root node).
    */
    LM_INTERFACE_F(Parent, const PropertyNode*());
    
public:

    #pragma region Type conversion functions

    template <typename T> auto As() const -> T;
    template <> auto As<const char*>() const -> const char* { return RawScalar(); }
    template <> auto As<std::string>() const -> std::string { return Scalar(); }
    template <> auto As<int>() const -> int { return std::stoi(Scalar()); }
    template <> auto As<double>() const -> double { return std::stod(Scalar()); }

    template <>
    auto As<Float>() const -> Float
    {
        return Float(As<double>());
    }

    template <>
    auto As<Vec3>() const -> Vec3
    {
        Vec3 v;
        std::stringstream ss(Scalar());
        double t;
        int i = 0;
        while (ss >> t) { v[i++] = Float(t); }
        return v;
    }

    template <>
    auto As<Vec4>() const -> Vec4
    {
        Vec4 v;
        std::stringstream ss(Scalar());
        double t;
        int i = 0;
        while (ss >> t) { v[i++] = Float(t); }
        return v;
    }

    template <>
    auto As<Mat3>() const -> Mat3
    {
        Mat3 m;
        std::stringstream ss(Scalar());
        double t;
        int i = 0;
        while (ss >> t) { m[i/3][i%3] = Float(t); i++; }
        return m;
    }

    template <>
    auto As<Mat4>() const -> Mat4
    {
        Mat4 m;
        std::stringstream ss(Scalar());
        double t;
        int i = 0;
        while (ss >> t) { m[i/4][i%4] = Float(t); i++; }
        return m;
    }

    template <>
    auto As<std::vector<Float>>() const -> std::vector<Float>
    {
        std::vector<Float> v;
        std::stringstream ss(Scalar());
        double t;
        while (ss >> t) { v.push_back(Float(t)); }
        return v;
    }

    template <>
    auto As<std::vector<unsigned int>>() const -> std::vector<unsigned int>
    {
        std::vector<unsigned int> v;
        std::stringstream ss(Scalar());
        unsigned int t;
        while (ss >> t) { v.push_back(t); }
        return v;
    }

    #pragma endregion

public:

    LM_INTERFACE_CLASS_END(PropertyNode);

};

/*!
    Property tree.
    
    Manages tree structure.
    Mainly utilized as asset parameters.
    This class manages all instances of the property nodes.
*/
class PropertyTree : public Component
{
public:

    LM_INTERFACE_CLASS(PropertyTree, Component);

public:

    PropertyTree() = default;
    LM_DISABLE_COPY_AND_MOVE(PropertyTree);

public:
    
    LM_INTERFACE_F(LoadFromFile, bool(const std::string&));

    /*!
        Load property from YAML sequences.
    */
    LM_INTERFACE_F(LoadFromString, bool(const std::string&));

    LM_INTERFACE_F(Path, std::string());

    /*!
        Get root node.
    */
    LM_INTERFACE_F(Root, const PropertyNode*());

public:

    LM_INTERFACE_CLASS_END(PropertyTree);

};

LM_NAMESPACE_END
