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

#define NANORT_IMPLEMENTATION

#if LM_COMPILER_MSVC

#pragma warning(push)
#pragma warning(disable:4244)
#pragma warning(disable:4267)
#pragma warning(disable:4456)
#pragma warning(disable:4018)
#pragma warning(disable:4245)
#pragma warning(disable:4305)
#include <nanort/nanort.h>
#pragma warning(pop)

#elif LM_COMPILER_GCC

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include <nanort/nanort.h>
#pragma GCC diagnostic pop

#else

#include <nanort/nanort.h>

#endif

LM_NAMESPACE_BEGIN

/*
    TODO: Falling tests. Fix it.
*/
class Accel_NanoRT final : public Accel3
{
public:

    LM_IMPL_CLASS(Accel_NanoRT, Accel3);

public:

    LM_IMPL_F(Initialize) = [this](const PropertyNode*) -> bool
    {
        // Do nothing
        return true;
    };

    LM_IMPL_F(Build) = [this](const Scene3* scene) -> bool
    {
        // Convert a set of primitives to one large mesh
        fsCDF_.push_back(0);
        for (int i = 0; i < scene->NumPrimitives(); i++)
        {
            const auto* prim = scene->PrimitiveAt(i);
            const auto* mesh = prim->mesh;
            if (!mesh)
            {
                continue;
            }

            const auto* ps = mesh->Positions();
            const auto* faces = mesh->Faces();
            const auto nps = (unsigned int)(ps_.size()) / 3;
            for (int j = 0; j < mesh->NumFaces(); j++)
            {
                // Positions
                unsigned int i1 = faces[3 * j];
                unsigned int i2 = faces[3 * j + 1];
                unsigned int i3 = faces[3 * j + 2];
                Vec3 p1(prim->transform * Vec4(ps[3 * i1], ps[3 * i1 + 1], ps[3 * i1 + 2], 1_f));
                Vec3 p2(prim->transform * Vec4(ps[3 * i2], ps[3 * i2 + 1], ps[3 * i2 + 2], 1_f));
                Vec3 p3(prim->transform * Vec4(ps[3 * i3], ps[3 * i3 + 1], ps[3 * i3 + 2], 1_f));

                // Store into the buffers
                fs_.push_back(nps + i1);
                fs_.push_back(nps + i2);
                fs_.push_back(nps + i3);
                ps_.push_back((float)(p1.x));
                ps_.push_back((float)(p1.y));
                ps_.push_back((float)(p1.z));
                ps_.push_back((float)(p2.x));
                ps_.push_back((float)(p2.y));
                ps_.push_back((float)(p2.z));
                ps_.push_back((float)(p3.x));
                ps_.push_back((float)(p3.y));
                ps_.push_back((float)(p3.z));
                faceIDToPrimitive_.push_back(i);
            }

            fsCDF_.push_back(fsCDF_.back() + mesh->NumFaces());
        }

        // Build
        nanort::BVHBuildOptions options;
        if (!accel_.Build(ps_.data(), fs_.data(), (unsigned int)(fs_.size()) / 3, options))
        {
            return false;
        }

        return true;
    };

    LM_IMPL_F(Intersect) = [this](const Scene3* scene, const Ray& ray, Intersection& isect, Float minT, Float maxT) -> bool
    {
        nanort::Ray rayRT;
        rayRT.org[0] = (float)(ray.o[0]);
        rayRT.org[1] = (float)(ray.o[1]);
        rayRT.org[2] = (float)(ray.o[2]);
        rayRT.dir[0] = (float)(ray.d[0]);
        rayRT.dir[1] = (float)(ray.d[1]);
        rayRT.dir[2] = (float)(ray.d[2]);

        nanort::Intersection isectRT;
        isectRT.t = (float)(maxT);

        nanort::BVHTraceOptions traceOptions;
        const bool hit = accel_.Traverse(isectRT, ps_.data(), fs_.data(), rayRT, traceOptions);
        if (!hit)
        {
            return false;
        }

        const int primIndex = faceIDToPrimitive_[isectRT.faceID];
        const int faceIndex = isectRT.faceID - fsCDF_[primIndex];

        isect = IntersectionUtils::CreateTriangleIntersection(
            scene->PrimitiveAt(faceIndex),
            ray.o + ray.d * (Float)(isectRT.t),
            Vec2(isectRT.u, isectRT.v),
            primIndex);

        return true;
    };

private:

    nanort::BVHAccel accel_;
    std::vector<float> ps_;
    std::vector<unsigned int> fs_;
    std::vector<unsigned int> faceIDToPrimitive_;
    std::vector<unsigned int> fsCDF_;

};

LM_COMPONENT_REGISTER_IMPL(Accel_NanoRT, "accel::nanort");

LM_NAMESPACE_END
