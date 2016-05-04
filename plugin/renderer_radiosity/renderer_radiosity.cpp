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

#include <lightmetrica/lightmetrica.h>
#include <stack>
#include <sstream>
#include <boost/format.hpp>
#if LM_COMPILER_MSVC
#pragma warning(disable:4714)
#endif
#include <Eigen/Sparse>

namespace Eigen
{
    template <>
    struct NumTraits<lightmetrica_v2::Vec3>
    {
        using T          = lightmetrica_v2::Vec3;
        using VT         = lightmetrica_v2::Float;
        using Real       = T;
        using NonInteger = T;
        using Nested     = T;
        enum
        {
            IsComplex = 0,
            IsInteger = 0,
            IsSigned = 1,
            RequireInitialization = 1,
            ReadCost = 3,
            AddCost = 3,
            MulCost = 3
        };
        static inline T epsilon()         { return T(NumTraits<VT>::epsilon()); }
        static inline T dummy_precision() { return T(NumTraits<VT>::dummy_precision()); }
        static inline T highest()         { return T(std::numeric_limits<VT>::max()); }
        static inline T lowest()          { return T(std::numeric_limits<VT>::min()); }
    };

    namespace internal
    {
        template<>
        struct significant_decimals_impl<lightmetrica_v2::Vec3>
        {
            static inline int run()
            {
                return significant_decimals_impl<lightmetrica_v2::Float>::run();
            }
        };
    }
}

LM_NAMESPACE_BEGIN

// For Eigen
// https://eigen.tuxfamily.org/dox/TopicCustomizingEigen.html
auto abs(const Vec3& v) -> Vec3 { return Vec3(std::abs(v.x), std::abs(v.y), std::abs(v.z)); }
auto sqrt(const Vec3& v) -> Vec3 { return Vec3(std::sqrt(v.x), std::sqrt(v.y), std::sqrt(v.z)); }
auto log(const Vec3& v) -> Vec3 { return Vec3(std::log(v.x), std::log(v.y), std::log(v.z)); }
auto ceil(const Vec3& v) -> Vec3 { return Vec3(std::ceil(v.x), std::ceil(v.y), std::ceil(v.z)); }
auto operator<<(std::ostream& os, const Vec3& v) -> std::ostream&
{
    os << "(" << v.x << "," << v.y << "," << v.z << ")";
    return os;
}

/*!
    \brief Radiosity renderer.
    
    Implements the radiosity algorithm by directly solving linear system.
    This implementation currently only supports the diffues BSDF (`bsdf::diffuse`)
    and the area light (`light::area`).

    References:
      - [Cohen & Wallace 1995] Radiosity and realistic image synthesis
      - [Willmott & Heckbert 1997] An empirical comparison of radiosity algorithms
*/
class Renderer_Radiosity final : public Renderer
{
public:

    LM_IMPL_CLASS(Renderer_Radiosity, Renderer);

public:

    LM_IMPL_F(Initialize) = [this](const PropertyNode* prop) -> bool
    {
        subdivLimitArea_ = prop->ChildAs<Float>("subdivlimitarea", 0.1_f);
        wireframe_ = prop->ChildAs<int>("wireframe", 0);
        return true;
    };

    LM_IMPL_F(Render) = [this](const Scene* scene, Film* film) -> void
    {
        #pragma region Create patches

        // As well as the patches, we create the quad tree associated to each triangle
        // to accelerate the query to get the patch index from the intersected point.

        struct Patch
        {
            const Primitive* primitive;
            Vec3 p1, p2, p3;
            Vec3 gn;
            auto Centroid() const -> Vec3 { return (p1 + p2 + p3) / 3_f; }
            auto Area() const -> Float { return Math::Length(Math::Cross(p2 - p1, p3 - p1)) * 0.5_f; }
        };

        struct QuadTreeNode
        {
            bool isleaf = false;
            union
            {
                struct
                {
                    
                } leaf;
                struct
                {
                    
                } internal;
            };
        };

        std::vector<Patch> patches;
        for (int i = 0; i < scene->NumPrimitives(); i++)
        {
            const auto* primitive = scene->PrimitiveAt(i);
            const auto* mesh = primitive->mesh;
            if (!mesh)
            {
                continue;
            }

            // --------------------------------------------------------------------------------

            #pragma region Check validity of the primitive

            if (primitive->light)
            {
                if (std::strcmp(primitive->light->implName, "Light_Area") != 0)
                {
                    LM_LOG_WARN(std::string(this->implName) + ": Non area light is found; skipping.");
                    continue;
                }
            }
            if (primitive->bsdf)
            {
                if (std::strcmp(primitive->bsdf->implName, "BSDF_Diffuse") != 0)
                {
                    LM_LOG_WARN(std::string(this->implName) + ": Non diffuse BSDF is found; skipping.");
                    continue;
                }
            }

            #pragma endregion

            // --------------------------------------------------------------------------------

            #pragma region Subdivide the triangles in the mesh

            const auto* ps = mesh->Positions();
            const auto* faces = mesh->Faces();
            for (int fi = 0; fi < primitive->mesh->NumFaces(); fi++)
            {
                unsigned int vi1 = faces[3 * fi];
                unsigned int vi2 = faces[3 * fi + 1];
                unsigned int vi3 = faces[3 * fi + 2];
                Vec3 p1(primitive->transform * Vec4(ps[3 * vi1], ps[3 * vi1 + 1], ps[3 * vi1 + 2], 1_f));
                Vec3 p2(primitive->transform * Vec4(ps[3 * vi2], ps[3 * vi2 + 1], ps[3 * vi2 + 2], 1_f));
                Vec3 p3(primitive->transform * Vec4(ps[3 * vi3], ps[3 * vi3 + 1], ps[3 * vi3 + 2], 1_f));
                const auto gn = Math::Normalize(Math::Cross(p2 - p1, p3 - p1));
                
                // --------------------------------------------------------------------------------

                #pragma region Subdivide the triangle

                struct Tri
                {
                    Vec3 p1, p2, p3;
                    auto Area() const -> Float { return Math::Length(Math::Cross(p2 - p1, p3 - p1)) * 0.5_f; }
                };

                std::stack<Tri> stack;
                stack.push({ p1, p2, p3 });
                while (!stack.empty())
                {
                    const auto tri = stack.top(); stack.pop();
                    if (tri.Area() < subdivLimitArea_)
                    {
                        patches.push_back({ primitive, tri.p1, tri.p2, tri.p3, gn });
                        continue;
                    }

                    const auto c1 = (tri.p1 + tri.p2) * 0.5_f;
                    const auto c2 = (tri.p2 + tri.p3) * 0.5_f;
                    const auto c3 = (tri.p3 + tri.p1) * 0.5_f;
                    stack.push({ tri.p1, c1, c3 });
                    stack.push({ tri.p2, c2, c1 });
                    stack.push({ tri.p3, c3, c2 });
                    stack.push({ c1, c2, c3 });
                }

                #pragma endregion
            }

            #pragma endregion
        }

        #pragma endregion

        // --------------------------------------------------------------------------------

        #pragma region Setup matrices

        const int N = (int)(patches.size());
        using Matrix = Eigen::SparseMatrix<Vec3>;
        using Vector = Eigen::Matrix<Vec3, Eigen::Dynamic, 1>;

        // Setup emission term
        Vector E(N);
        for (int i = 0; i < N; i++)
        {
            const auto& patch = patches[i];
            const auto* light = patch.primitive->light;
            if (!light)
            {
                continue;
            }
            E(i) = light->Emittance().ToRGB();
        }

        // Helper function to estimate the form factor
        const auto EstimateFromFactor = [&](int i, int j) -> Float
        {
            const auto& pi = patches[i];
            const auto& pj = patches[j];
            const auto  ci = pi.Centroid();
            const auto  cj = pj.Centroid();

            // Check if two patches area facing each other
            const auto cij = Math::Normalize(cj - ci);
            const auto cji = -cij;
            const auto cosThetai = Math::Dot(pi.gn, cij);
            const auto cosThetaj = Math::Dot(pj.gn, cji);
            if (cosThetai <= 0_f || cosThetaj <= 0_f)
            {
                return 0_f;
            }

            // Check visibility
            if (!scene->Visible(ci, cj))
            {
                return 0_f;
            }
            
            // Point-to-point estimate (Eq.4 in [Willmott & Heckbert 1997])
            return pj.Area() * cosThetai * cosThetaj / Math::Pi() / Math::Length2(ci - cj);
        };

        // Setup matrix of interactions
        Matrix K(N, N);
        K.setIdentity();
        for (int i = 0; i < N; i++)
        {
            for (int j = 0; j < N; j++)
            {
                const auto Fij = EstimateFromFactor(i, j);
                if (Fij > 0_f)
                {
                    K.coeffRef(i, j) -= patches[i].primitive->bsdf->Reflectance().ToRGB() * Fij;
                }
            }
        }

        #pragma endregion

        // --------------------------------------------------------------------------------

        #pragma region Solve radiosity equation

        Eigen::BiCGSTAB<Matrix> solver;
        solver.compute(K);
        Vector B = solver.solve(E);

        std::stringstream ss;
        ss << B << std::endl;
        LM_LOG_INFO(ss.str());

        #pragma endregion

        // --------------------------------------------------------------------------------

        #pragma region Rendering (ray casting)

        const int width  = film->Width();
        const int height = film->Height();
        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                // Raster position
                Vec2 rasterPos((Float(x) + 0.5_f) / Float(width), (Float(y) + 0.5_f) / Float(height));

                // Position and direction of a ray
                SurfaceGeometry geomE;
                Vec3 wo;
                scene->Sensor()->emitter->SamplePositionAndDirection(rasterPos, Vec2(), geomE, wo);

                // Setup a ray
                Ray ray = { geomE.p, wo };

                // Intersection query
                Intersection isect;
                if (!scene->Intersect(ray, isect))
                {
                    // No intersection -> black
                    film->SetPixel(x, y, SPD());
                    continue;
                }
                
                // --------------------------------------------------------------------------------

                #pragma region Compute patch index & visualize the radiosity

                // For this requirement, what we want to do here is to subdivide and
                // check if the query point is in the triangle. This actually increase the complexity
                // compared with the case that we use a quad-tree associated with each triangle for example, 
                // but I think the ray casting is fast enough and this does not degrade performance so much.
                {
                    const auto* mesh = isect.primitive->mesh;
                    const auto* fs = mesh->Faces();
                    const auto* ps = mesh->Positions();
                    int v1 = fs[3 * isect.geom.faceindex];
                    int v2 = fs[3 * isect.geom.faceindex + 1];
                    int v3 = fs[3 * isect.geom.faceindex + 2];
                    Vec3 p1(isect.primitive->transform * Vec4(ps[3 * v1], ps[3 * v1 + 1], ps[3 * v1 + 2], 1_f));
                    Vec3 p2(isect.primitive->transform * Vec4(ps[3 * v2], ps[3 * v2 + 1], ps[3 * v2 + 2], 1_f));
                    Vec3 p3(isect.primitive->transform * Vec4(ps[3 * v3], ps[3 * v3 + 1], ps[3 * v3 + 2], 1_f));

                    // --------------------------------------------------------------------------------

                    struct Tri
                    {
                        Vec3 p1, p2, p3;
                        auto Area() const -> Float { return Math::Length(Math::Cross(p2 - p1, p3 - p1)) * 0.5_f; }
                    };

                    int patchindex = 0;
                    std::stack<Tri> stack;
                    stack.push({ p1, p2, p3 });
                    while (!stack.empty())
                    {
                        const auto tri = stack.top(); stack.pop();
                        if (tri.Area() < subdivLimitArea_)
                        {
                            // Check if the intersected point is in the subdivided triangle
                            const auto e0 = tri.p3 - tri.p1;
                            const auto e1 = tri.p2 - tri.p1;
                            const auto e2 = isect.geom.p - tri.p1;
                            const auto dot00 = Math::Dot(e0, e0);
                            const auto dot01 = Math::Dot(e0, e1);
                            const auto dot02 = Math::Dot(e0, e2);
                            const auto dot11 = Math::Dot(e1, e1);
                            const auto dot12 = Math::Dot(e1, e2);
                            const auto invDenom = 1_f / (dot00 * dot11 - dot01 * dot01);
                            const auto u = (dot11 * dot02 - dot01 * dot12) * invDenom;
                            const auto v = (dot00 * dot12 - dot01 * dot02) * invDenom;
                            if (0_f < u && 0_f < v && u + v < 1_f)
                            {
                                if (wireframe_)
                                {
                                    // Visualize wire frame
                                    // Compute minimum distance from each edges
                                    const auto mind = Math::Min(u, Math::Min(v, 1_f - u - v));
                                    if (mind < 0.05f)
                                    {
                                        film->SetPixel(x, y, SPD(Math::Abs(Math::Dot(isect.geom.sn, -ray.d))));
                                    }
                                }
                                else
                                {
                                    film->SetPixel(x, y, SPD(B(patchindex)));
                                    if (patchindex == 2)
                                    {
                                        __debugbreak();
                                    }
                                }
                            }

                            patchindex++;
                            continue;
                        }

                        const auto c1 = (tri.p1 + tri.p2) * 0.5_f;
                        const auto c2 = (tri.p2 + tri.p3) * 0.5_f;
                        const auto c3 = (tri.p3 + tri.p1) * 0.5_f;
                        stack.push({ tri.p1, c1, c3 });
                        stack.push({ tri.p2, c2, c1 });
                        stack.push({ tri.p3, c3, c2 });
                        stack.push({ c1, c2, c3 });
                    }
                }

                #pragma endregion
            }

            const double progress = 100.0 * y / film->Height();
            LM_LOG_INPLACE(boost::str(boost::format("Progress: %.1f%%") % progress));
        }

        LM_LOG_INFO("Progress: 100.0%");

        #pragma endregion
    };

private:

    Float subdivLimitArea_;
    int wireframe_;

};

LM_COMPONENT_REGISTER_IMPL(Renderer_Radiosity, "renderer::radiosity");

LM_NAMESPACE_END
