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

#include <lightmetrica/accel.h>
#include <lightmetrica/scene.h>
#include <lightmetrica/property.h>
#include <lightmetrica/ray.h>
#include <lightmetrica/intersection.h>
#include <lightmetrica/primitive.h>
#include <lightmetrica/trianglemesh.h>

LM_NAMESPACE_BEGIN

struct TriAccelTriangle
{

    uint32_t k;
    Float n_u;
    Float n_v;
    Float n_d;

    Float a_u;
    Float a_v;
    Float b_nu;
    Float b_nv;

    Float c_nu;
    Float c_nv;
    uint32_t faceIndex;
    uint32_t primIndex;

    auto Load(const Vec3& A, const Vec3& B, const Vec3& C) -> int
    {
        static const int waldModulo[4] = { 1, 2, 0, 1 };

        Vec3 b = C - A;
        Vec3 c = B - A;
        Vec3 N = Math::Cross(c, b);

        k = 0;
        // Determine the largest projection axis
        for (int j = 0; j < 3; j++)
        {
            if (Math::Abs(N[j]) > Math::Abs(N[k]))
            {
                k = j;
            }
        }

        uint32_t u = waldModulo[k];
        uint32_t v = waldModulo[k + 1];
        const Float n_k = N[k];
        const Float denom = b[u] * c[v] - b[v] * c[u];

        if (denom == 0)
        {
            k = 3;
            return 1;
        }

        // Pre-compute intersection calculation constants
        n_u = N[u] / n_k;
        n_v = N[v] / n_k;
        n_d = Math::Dot(Vec3(A), N) / n_k;
        b_nu = b[u] / denom;
        b_nv = -b[v] / denom;
        a_u = A[u];
        a_v = A[v];
        c_nu = c[v] / denom;
        c_nv = -c[u] / denom;

        return 0;
    }

    auto Intersect(const Ray& ray, Float mint, Float maxt, Float& u, Float& v, Float& t) const -> bool
    {
        Float o_u, o_v, o_k, d_u, d_v, d_k;
        switch (k)
        {
            case 0:
                o_u = ray.o[1];
                o_v = ray.o[2];
                o_k = ray.o[0];
                d_u = ray.d[1];
                d_v = ray.d[2];
                d_k = ray.d[0];
                break;

            case 1:
                o_u = ray.o[2];
                o_v = ray.o[0];
                o_k = ray.o[1];
                d_u = ray.d[2];
                d_v = ray.d[0];
                d_k = ray.d[1];
                break;

            case 2:
                o_u = ray.o[0];
                o_v = ray.o[1];
                o_k = ray.o[2];
                d_u = ray.d[0];
                d_v = ray.d[1];
                d_k = ray.d[2];
                break;

            default:
                return false;
        }


        #if LM_DEBUG_MODE
        if (d_u * n_u + d_v * n_v + d_k == 0)
        {
            return false;
        }
        #endif

        // Calculate the plane intersection (Typo in the thesis?)
        t = (n_d - o_u*n_u - o_v*n_v - o_k) / (d_u * n_u + d_v * n_v + d_k);
        if (t < mint || t > maxt)
        {
            return false;
        }

        // Calculate the projected plane intersection point
        const Float hu = o_u + t * d_u - a_u;
        const Float hv = o_v + t * d_v - a_v;

        // In barycentric coordinates
        u = hv * b_nu + hu * b_nv;
        v = hu * c_nu + hv * c_nv;

        return u >= 0 && v >= 0 && u + v <= 1;
    }

};

/*!
    \brief Naive acceleration structure.

    Almost-do-nothing acceleration structure.
    We simply utilizes a list of triangles from the primitives as a structure.
    We utilize this class only for testing, not recommend in the practical use.
*/
class Accel_Naive : public Accel
{
public:

    LM_IMPL_CLASS(Accel_Naive, Accel);

public:

    LM_IMPL_F(Initialize) = [this](const PropertyNode*) -> bool
    {
        // Do nothing
        return true;
    };

    LM_IMPL_F(Build) = [this](const Scene& scene) -> bool
    {
        //LM_LOG_BEGIN_PROGRESS();
        //signal_ReportBuildProgress(0, false);

        int np = scene.NumPrimitives();
        for (int i = 0; i < np; i++)
        {
            const auto* prim = scene.PrimitiveAt(i);
            const auto* mesh = prim->mesh;
            if (mesh)
            {
                // Enumerate all triangles and create triaccels
                const auto* ps = mesh->Positions();
                const auto* faces = mesh->Faces();
                for (int j = 0; j < mesh->NumFaces() / 3; j++)
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

            //LM_LOG_BEGIN_TICK_PROGRESS();
            //signal_ReportBuildProgress(static_cast<double>(i) / numPrimitives, i == numPrimitives - 1);
        }

        //LM_LOG_END_PROGRESS();

        return true;
    };

    LM_IMPL_F(Intersect) = [this](const Scene& scene, const Ray& ray, Intersection& isect) -> bool
    {
        bool intersected = false;
        size_t minIndex = 0;
        Vec2 minB;
        Float maxT = Math::Inf<Float>();

        for (size_t i = 0; i < triangles_.size(); i++)
        {
            Float t;
            Vec2 b;
            if (triangles_[i].Intersect(ray, 0, maxT, b[0], b[1], t))
            {
                intersected = true;
                maxT = t;
                minIndex = i;
                minB = b;
            }
        }

        if (intersected)
        {
            // Store required data for the intersection structure
            const auto& triangle = triangles_[minIndex];
            const auto& b = minB;

            // Store information into `isect`
            const size_t primIndex = triangle.primIndex;
            const int faceIndex = triangle.faceIndex;
            const auto* prim = scene.PrimitiveAt(primIndex);
            const auto* mesh = prim->mesh;
            isect.primitive = prim;

            // Intersection point
            isect.geom.p = ray.o + ray.d * maxT;

            // Face indices
            const auto* fs = mesh->Faces();
            int v1 = fs[3 * faceIndex];
            int v2 = fs[3 * faceIndex + 1];
            int v3 = fs[3 * faceIndex + 2];

            // Geometry normal
            const auto* ps = mesh->Positions();
            Vec3 p1(ps[3 * v1], ps[3 * v1 + 1], ps[3 * v1 + 2]);
            Vec3 p2(ps[3 * v2], ps[3 * v2 + 1], ps[3 * v2 + 2]);
            Vec3 p3(ps[3 * v3], ps[3 * v3 + 1], ps[3 * v3 + 2]);
            isect.geom.gn = Math::Normalize(Math::Cross(p2 - p1, p3 - p1));

            // Shading normal
            const auto* ns = mesh->Normals();
            Vec3 n1(ns[3 * v1], ns[3 * v1 + 1], ns[3 * v1 + 2]);
            Vec3 n2(ns[3 * v2], ns[3 * v2 + 1], ns[3 * v2 + 2]);
            Vec3 n3(ns[3 * v3], ns[3 * v3 + 1], ns[3 * v3 + 2]);
            isect.geom.sn = Math::Normalize(n1 * (1_f - b[0] - b[1]) + n2 * b[0] + n3 * b[1]);
            if (std::isnan(isect.geom.sn.x) || std::isnan(isect.geom.sn.y) || std::isnan(isect.geom.sn.z))
            {
                // There is a case with one of n1 ~ n3 generates NaN
                // possibly a bug of mesh loader?
                isect.geom.sn = isect.geom.gn;
            }

            // Texture coordinates
            const auto* uvs = mesh->Texcoords();
            if (uvs)
            {
                Vec2 uv1(uvs[2 * v1], uvs[2 * v1 + 1]);
                Vec2 uv2(uvs[2 * v2], uvs[2 * v2 + 1]);
                Vec2 uv3(uvs[2 * v3], uvs[2 * v3 + 1]);
                isect.geom.uv = uv1 * (1_f - b[0] - b[1]) + uv2 * b[0] + uv3 * b[1];
            }

            // Scene surface is not degenerated
            isect.geom.degenerated = false;

            // Compute tangent space
            Math::OrthonormalBasis(isect.geom.sn, isect.geom.dpdu, isect.geom.dpdv);
            isect.geom.ToWorld = Mat3(isect.geom.dpdu, isect.geom.dpdv, isect.geom.sn);
            isect.geom.ToLocal = Math::Transpose(isect.geom.ToWorld);

            // Compute normal derivative
            const auto N = n1 * (1_f - b[0] - b[1]) + n2 * b[0] + n3 * b[1];
            const auto invNLen = 1_f / Math::Length(N);
            const auto dNdu = (n2 - n1) * invNLen;
            const auto dNdv = (n3 - n2) * invNLen;
            isect.geom.dndu = dNdu - isect.geom.sn * Math::Dot(dNdu, isect.geom.sn);
            isect.geom.dndv = dNdv - isect.geom.sn * Math::Dot(dNdv, isect.geom.sn);
        }

        return intersected;
    };

private:

    std::vector<TriAccelTriangle> triangles_;

};

LM_COMPONENT_REGISTER_IMPL(Accel_Naive, "naiveaccel");

LM_NAMESPACE_END