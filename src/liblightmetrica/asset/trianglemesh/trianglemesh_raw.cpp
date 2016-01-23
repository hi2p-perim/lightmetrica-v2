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
        ps = prop->Child("positions")->As<std::vector<Float>>();
        if (ps.size() % 3 != 0)
        {
            LM_LOG_ERROR("Invalid number of elements in 'positions': " + std::to_string(ps.size()));
            PropertyUtils::PrintPrettyError(prop->Child("positions"));
            return false;
        }

        ns = prop->Child("normals")->As<std::vector<Float>>();
        if (ns.size() != ps.size())
        {
            LM_LOG_ERROR("Invalid number of elements in 'normals': " + std::to_string(ns.size()));
            PropertyUtils::PrintPrettyError(prop->Child("normals"));
            return false;
        }

        if (prop->Child("texcoords"))
        {
            ts = prop->Child("texcoords")->As<std::vector<Float>>();
            if (ts.size() != ps.size() / 3 * 2)
            {
                LM_LOG_ERROR("Invalid number of elements in 'normals': " + std::to_string(ts.size()));
                PropertyUtils::PrintPrettyError(prop->Child("texcoords"));
                return false;
            }
        }

        fs = prop->Child("faces")->As<std::vector<unsigned int>>();
        if (fs.size() % 3 != 0)
        {
            LM_LOG_ERROR("Invalid number of elements in 'faces': " + std::to_string(fs.size()));
            PropertyUtils::PrintPrettyError(prop->Child("faces"));
            return false;
        }

        return true;
    };

    LM_IMPL_F(NumVertices) = [this]() -> int { return (int)(ps.size()) / 3; };
    LM_IMPL_F(NumFaces)    = [this]() -> int { return (int)(fs.size()) / 3; };
    LM_IMPL_F(Positions)   = [this]() -> const Float*{ return ps.data(); };
    LM_IMPL_F(Normals)     = [this]() -> const Float*{ return ns.data(); };
    LM_IMPL_F(Texcoords)   = [this]() -> const Float*{ return ts.data(); };
    LM_IMPL_F(Faces)       = [this]() -> const unsigned int* { return fs.data(); };

protected:

    std::vector<Float> ps;
    std::vector<Float> ns;
    std::vector<Float> ts;
    std::vector<unsigned int> fs;

};

LM_COMPONENT_REGISTER_IMPL(TriangleMesh_Raw, "raw");

LM_NAMESPACE_END
