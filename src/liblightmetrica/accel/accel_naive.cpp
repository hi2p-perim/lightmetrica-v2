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
#include <lightmetrica/accel.h>
#include <lightmetrica/scene.h>
#include <lightmetrica/property.h>
#include <lightmetrica/ray.h>
#include <lightmetrica/intersection.h>
#include <lightmetrica/primitive.h>
#include <lightmetrica/trianglemesh.h>
#include <lightmetrica/intersectionutils.h>
#include <lightmetrica/triaccel.h>

LM_NAMESPACE_BEGIN

/*!
    \brief Naive acceleration structure.

    Almost-do-nothing acceleration structure.
    We simply utilizes a list of triangles from the primitives as a structure.
    We utilize this class only for testing, not recommend in the practical use.
*/
class Accel_Naive final : public Accel
{
public:

    LM_IMPL_CLASS(Accel_Naive, Accel);

public:

    LM_IMPL_F(Initialize) = [this](const PropertyNode*) -> bool
    {
        // Do nothing
        return true;
    };

    LM_IMPL_F(Build) = [this](const Scene* scene) -> bool
    {
        int np = scene->NumPrimitives();
        for (int i = 0; i < np; i++)
        {
            const auto* prim = scene->PrimitiveAt(i);
            const auto* mesh = prim->mesh;
            if (mesh)
            {
                // Enumerate all triangles and create triaccels
                const auto* ps = mesh->Positions();
                const auto* faces = mesh->Faces();
                for (int j = 0; j < mesh->NumFaces(); j++)
                {
                    // Create a triaccel
                    triangles_.push_back(TriAccelTriangle());
                    triangles_.back().faceIndex = j;
                    triangles_.back().primIndex = i;
                    unsigned int i1 = faces[3 * j];
                    unsigned int i2 = faces[3 * j + 1];
                    unsigned int i3 = faces[3 * j + 2];
                    Vec3 p1(prim->transform * Vec4(ps[3 * i1], ps[3 * i1 + 1], ps[3 * i1 + 2], 1_f));
                    Vec3 p2(prim->transform * Vec4(ps[3 * i2], ps[3 * i2 + 1], ps[3 * i2 + 2], 1_f));
                    Vec3 p3(prim->transform * Vec4(ps[3 * i3], ps[3 * i3 + 1], ps[3 * i3 + 2], 1_f));
                    triangles_.back().Load(p1, p2, p3);
                }
            }
        }

        return true;
    };

    LM_IMPL_F(Intersect) = [this](const Scene* scene, const Ray& ray, Intersection& isect, Float minT, Float maxT) -> bool
    {
        bool intersected = false;
        size_t minIndex = 0;
        Vec2 minB;

        for (size_t i = 0; i < triangles_.size(); i++)
        {
            Float t;
            Vec2 b;
            if (triangles_[i].Intersect(ray, minT, maxT, b[0], b[1], t))
            {
                intersected = true;
                maxT = t;
                minIndex = i;
                minB = b;
            }
        }

        if (!intersected)
        {
            return false;
        }

        isect = IntersectionUtils::CreateTriangleIntersection(
            scene->PrimitiveAt(triangles_[minIndex].primIndex),
            ray.o + ray.d * maxT,
            minB,
            triangles_[minIndex].faceIndex);

        return true;
    };

private:

    std::vector<TriAccelTriangle> triangles_;

};

LM_COMPONENT_REGISTER_IMPL(Accel_Naive, "accel::naive");

LM_NAMESPACE_END
