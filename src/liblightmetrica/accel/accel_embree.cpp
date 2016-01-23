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

#include <embree2/rtcore.h>
#include <embree2/rtcore_ray.h>

LM_NAMESPACE_BEGIN

namespace
{
    auto EmbreeErrorHandler(const RTCError code, const char* str) -> void
    {
        std::string error = "";
        switch (code)
        {
            case RTC_UNKNOWN_ERROR:     { error = "RTC_UNKNOWN_ERROR";      break; }
            case RTC_INVALID_ARGUMENT:  { error = "RTC_INVALID_ARGUMENT";   break; }
            case RTC_INVALID_OPERATION: { error = "RTC_INVALID_OPERATION";  break; }
            case RTC_OUT_OF_MEMORY:     { error = "RTC_OUT_OF_MEMORY";      break; }
            case RTC_UNSUPPORTED_CPU:   { error = "RTC_UNSUPPORTED_CPU";    break; }
            default:                    { error = "Invalid error code";	    break; }
        }
        LM_LOG_ERROR("Embree error : " + error);
    }
}

class Accel_Embree : public Accel
{
public:

    LM_IMPL_CLASS(Accel_Embree, Accel);

public:

    Accel_Embree()
    {
        rtcInit(nullptr);
        rtcSetErrorFunction(EmbreeErrorHandler);
    }

    ~Accel_Embree()
    {
        if (RtcScene) rtcDeleteScene(RtcScene);
        rtcExit();
    }

public:

    LM_IMPL_F(Initialize) = [this](const PropertyNode*) -> bool
    {
        // Do nothing
        return true;
    };

    LM_IMPL_F(Build) = [this](const Scene* scene) -> bool
    {
        // Create scene
        RtcScene = rtcNewScene(RTC_SCENE_STATIC | RTC_SCENE_INCOHERENT, RTC_INTERSECT1);

        // Add meshes to the scene
        int np = scene->NumPrimitives();
        for (int i = 0; i < np; i++)
        {
            const auto* prim = scene->PrimitiveAt(i);
            const auto* mesh = prim->mesh;
            if (!mesh)
            {
                continue;
            }

            // Create a triangle mesh
            unsigned int geomID = rtcNewTriangleMesh(RtcScene, RTC_GEOMETRY_STATIC, mesh->NumFaces(), mesh->NumFaces() * 3);
            RtcGeomIDToPrimitiveIndexMap[geomID] = i;

            // Copy vertices & faces
            auto* mappedPositions = reinterpret_cast<float*>(rtcMapBuffer(RtcScene, geomID, RTC_VERTEX_BUFFER));
            auto* mappedFaces = reinterpret_cast<int*>(rtcMapBuffer(RtcScene, geomID, RTC_INDEX_BUFFER));

            const auto* ps = mesh->Positions();
            const auto* faces = mesh->Faces();
            for (int j = 0; j < mesh->NumFaces(); j++)
            {
                // Positions
                unsigned int i1 = faces[3 * j];
                unsigned int i2 = faces[3 * j + 1];
                unsigned int i3 = faces[3 * j + 2];
                Vec3 p1(prim->transform * Vec4(ps[3 * i1], ps[3 * i1 + 1], ps[3 * i1 + 2], 1_f));
                Vec3 p2(prim->transform * Vec4(ps[3 * i2], ps[3 * i2 + 1], ps[3 * i2 + 2], 1_f));
                Vec3 p3(prim->transform * Vec4(ps[3 * i3], ps[3 * i3 + 1], ps[3 * i3 + 2], 1_f));

                // Store into mapped buffers
                int mi1 = 3 * (int)(j);
                int mi2 = 3 * (int)(j)+1;
                int mi3 = 3 * (int)(j)+2;
                mappedFaces[mi1] = mi1;
                mappedFaces[mi2] = mi2;
                mappedFaces[mi3] = mi3;
                for (int k = 0; k < 3; k++)
                {
                    mappedPositions[4 * mi1 + k] = p1[k];
                    mappedPositions[4 * mi2 + k] = p2[k];
                    mappedPositions[4 * mi3 + k] = p3[k];
                }
            }

            rtcUnmapBuffer(RtcScene, geomID, RTC_VERTEX_BUFFER);
            rtcUnmapBuffer(RtcScene, geomID, RTC_INDEX_BUFFER);
        }

        rtcCommit(RtcScene);

        return true;
    };

    LM_IMPL_F(Intersect) = [this](const Scene* scene, const Ray& ray, Intersection& isect, Float minT, Float maxT) -> bool
    {
        // Create RTCRay
        RTCRay rtcRay;
        rtcRay.org[0] = (float)(ray.o[0]);
        rtcRay.org[1] = (float)(ray.o[1]);
        rtcRay.org[2] = (float)(ray.o[2]);
        rtcRay.dir[0] = (float)(ray.d[0]);
        rtcRay.dir[1] = (float)(ray.d[1]);
        rtcRay.dir[2] = (float)(ray.d[2]);
        rtcRay.tnear = minT;
        rtcRay.tfar = maxT;
        rtcRay.geomID = RTC_INVALID_GEOMETRY_ID;
        rtcRay.primID = RTC_INVALID_GEOMETRY_ID;
        rtcRay.instID = RTC_INVALID_GEOMETRY_ID;
        rtcRay.mask = 0xFFFFFFFF;
        rtcRay.time = 0;

        // Intersection query
        //LM_DISABLE_FP_EXCEPTION();     // TODO: push
        rtcIntersect(RtcScene, rtcRay);
        //LM_ENABLE_FP_EXCEPTION();      // TODO: pop
        if ((unsigned int)(rtcRay.geomID) == RTC_INVALID_GEOMETRY_ID)
        {
            return false;
        }

        // Fill in the intersection structure
        isect = IntersectionUtils::CreateTriangleIntersection(
            scene->PrimitiveAt((int)(RtcGeomIDToPrimitiveIndexMap.at(rtcRay.geomID))),
            ray.o + ray.d * (Float)(rtcRay.tfar),
            Vec2(rtcRay.u, rtcRay.v),
            rtcRay.primID);

        return true;
    };

private:

    RTCScene RtcScene = nullptr;
    std::unordered_map<unsigned int, size_t> RtcGeomIDToPrimitiveIndexMap;

};

LM_COMPONENT_REGISTER_IMPL(Accel_Embree, "accel::embree");

LM_NAMESPACE_END
