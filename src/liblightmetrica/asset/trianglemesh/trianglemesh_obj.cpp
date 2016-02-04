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

#define TINYOBJLOADER_IMPLEMENTATION
#pragma warning(push)
#pragma warning(disable:4706)
#include <tinyobjloader/tiny_obj_loader.h>
#pragma warning(pop)

LM_NAMESPACE_BEGIN

class TriangleMesh_Obj final : public TriangleMesh
{
public:

    LM_IMPL_CLASS(TriangleMesh_Obj, TriangleMesh);

public:

    LM_IMPL_F(Load) = [this](const PropertyNode* prop, Assets* assets, const Primitive* primitive) -> bool
    {
        const auto localpath = prop->Child("path")->As<std::string>();
        const auto basepath = boost::filesystem::path(prop->Tree()->Path()).parent_path();
        const auto path = basepath / localpath;

        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;

        std::string err;
        bool ret = tinyobj::LoadObj(shapes, materials, err, path.string().c_str(), nullptr);
        if (!ret)
        {
            LM_LOG_ERROR(err);
            return false;
        }

        // Obj model must contain at least one shape
        assert(!shapes.empty());

        // There must not be the shape with normals and the shape with no normals in the same model
        bool nonormal   = shapes[0].mesh.normals.empty();
        bool notexcoord = shapes[0].mesh.texcoords.empty();
        for (size_t i = 1; i < shapes.size(); i++)
        {
            if (nonormal != shapes[i].mesh.normals.empty() || notexcoord != shapes[i].mesh.texcoords.empty())
            {
                LM_LOG_ERROR("Inconsistency of normal or texcoords");
                return false;
            }
        }

        // Copy
        for (const auto& shape : shapes)
        {
            const auto& mesh = shape.mesh;
            const size_t psN = ps_.size() / 3;
            ps_.insert(ps_.begin(), mesh.positions.begin(), mesh.positions.end());
            ns_.insert(ns_.begin(), mesh.normals.begin(), mesh.normals.end());
            ts_.insert(ts_.begin(), mesh.texcoords.begin(), mesh.texcoords.end());
            std::transform(mesh.indices.begin(), mesh.indices.end(), std::back_inserter(fs_), [&](unsigned int i)
            {
                return i + (unsigned int)(psN);
            });
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

LM_COMPONENT_REGISTER_IMPL(TriangleMesh_Obj, "trianglemesh::obj");

LM_NAMESPACE_END
