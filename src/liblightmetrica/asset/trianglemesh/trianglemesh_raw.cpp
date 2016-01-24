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
#include <lightmetrica/trianglemesh.h>
#include <lightmetrica/property.h>
#include <lightmetrica/detail/propertyutils.h>

LM_NAMESPACE_BEGIN

class TriangleMesh_Raw : public TriangleMesh
{
public:

    LM_IMPL_CLASS(TriangleMesh_Raw, TriangleMesh);

public:

    LM_IMPL_F(Load) = [this](const PropertyNode* prop, Assets* assets, const Primitive* primitive) -> bool
    {
        ps_ = prop->Child("positions")->As<std::vector<Float>>();
        if (ps_.size() % 3 != 0)
        {
            LM_LOG_ERROR("Invalid number of elements in 'positions': " + std::to_string(ps_.size()));
            PropertyUtils::PrintPrettyError(prop->Child("positions"));
            return false;
        }

        ns_ = prop->Child("normals")->As<std::vector<Float>>();
        if (ns_.size() != ps_.size())
        {
            LM_LOG_ERROR("Invalid number of elements in 'normals': " + std::to_string(ns_.size()));
            PropertyUtils::PrintPrettyError(prop->Child("normals"));
            return false;
        }

        if (prop->Child("texcoords"))
        {
            ts_ = prop->Child("texcoords")->As<std::vector<Float>>();
            if (ts_.size() != ps_.size() / 3 * 2)
            {
                LM_LOG_ERROR("Invalid number of elements in 'normals': " + std::to_string(ts_.size()));
                PropertyUtils::PrintPrettyError(prop->Child("texcoords"));
                return false;
            }
        }

        fs_ = prop->Child("faces")->As<std::vector<unsigned int>>();
        if (fs_.size() % 3 != 0)
        {
            LM_LOG_ERROR("Invalid number of elements in 'faces': " + std::to_string(fs_.size()));
            PropertyUtils::PrintPrettyError(prop->Child("faces"));
            return false;
        }

        return true;
    };

public:

    LM_IMPL_F(NumVertices) = [this]() -> int { return (int)(ps_.size()) / 3; };
    LM_IMPL_F(NumFaces)    = [this]() -> int { return (int)(fs_.size()) / 3; };
    LM_IMPL_F(Positions)   = [this]() -> const Float* { return ps_.data(); };
    LM_IMPL_F(Normals)     = [this]() -> const Float* { return ns_.data(); };
    LM_IMPL_F(Texcoords)   = [this]() -> const Float* { return ts_.data(); };
    LM_IMPL_F(Faces)       = [this]() -> const unsigned int* { return fs_.data(); };

protected:

    std::vector<Float> ps_;
    std::vector<Float> ns_;
    std::vector<Float> ts_;
    std::vector<unsigned int> fs_;

};

LM_COMPONENT_REGISTER_IMPL(TriangleMesh_Raw, "trianglemesh::raw");

LM_NAMESPACE_END
