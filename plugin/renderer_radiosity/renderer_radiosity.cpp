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
#include "radiosityutils.h"
#include <sstream>
#include <boost/format.hpp>
#if LM_COMPILER_MSVC
#pragma warning(disable:4714)
#endif
#include <Eigen/Sparse>

#define LM_RADIOSITY_DEBUG 0

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
      - [Schroder & Hanrahan 1993] On the form factor between two polygons
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
        analyticalFormFactor_ = prop->ChildAs<int>("analyticalformfactor", 0);
        return true;
    };

    LM_IMPL_F(Render) = [this](const Scene* scene, Random* initRng, Film* film) -> void
    {
        // Create patches
        Patches patches;
        patches.Create(scene, subdivLimitArea_);

        // --------------------------------------------------------------------------------

        #pragma region Setup matrices

        LM_LOG_INFO("Setup matrix");

        const int N = patches.Size();
        //using Matrix = Eigen::SparseMatrix<Vec3>;
        using Matrix = Eigen::Matrix<Vec3, Eigen::Dynamic, Eigen::Dynamic>;
        using Vector = Eigen::Matrix<Vec3, Eigen::Dynamic, 1>;

        // Setup emission term
        Vector E(N);
        for (int i = 0; i < N; i++)
        {
            const auto& patch = patches.At(i);
            const auto* light = patch.primitive->light;
            if (!light)
            {
                continue;
            }
            E(i) = light->Emittance().ToRGB();
        }

        // Setup matrix of interactions
        Matrix K(N, N);
        K.setIdentity();
        for (int i = 0; i < N; i++)
        {
            for (int j = 0; j < N; j++)
            {
                const auto Fij = RadiosityUtils::EstimateFormFactor(scene, patches.At(i), patches.At(j), analyticalFormFactor_ ? true : false);
                if (Fij > 0_f)
                {
                    K.coeffRef(i, j) -= patches.At(i).primitive->bsdf->Reflectance().ToRGB() * Fij;
                }
            }

            const double progress = 100.0 * i / N;
            LM_LOG_INPLACE(boost::str(boost::format("Progress: %.1f%%") % progress));
        }

        LM_LOG_INFO("Progress: 100.0%");

        #pragma endregion

        // --------------------------------------------------------------------------------

        #pragma region Solve radiosity equation

        LM_LOG_INFO("Solving linear system");

        Eigen::BiCGSTAB<Matrix> solver;
        solver.compute(K);
        Vector B = solver.solve(E);

        #if LM_RADIOSITY_DEBUG
        std::stringstream ss;
        ss << B << std::endl;
        LM_LOG_INFO(ss.str());
        #endif

        #pragma endregion

        // --------------------------------------------------------------------------------

        #pragma region Rendering (ray casting)

        LM_LOG_INFO("Visualizing result");

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
                scene->GetSensor()->emitter->SamplePositionAndDirection(rasterPos, Vec2(), geomE, wo);

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
                
                // Compute patch index & visualize the radiosity
                patches.IteratePatches(isect, subdivLimitArea_, [&](size_t patchindex, const Vec2& uv) -> void
                {
                    if (wireframe_)
                    {
                        // Visualize wire frame
                        // Compute minimum distance from each edges
                        const auto mind = Math::Min(uv.x, Math::Min(uv.y, 1_f - uv.x - uv.y));
                        if (mind < 0.05f)
                        {
                            film->SetPixel(x, y, SPD(Math::Abs(Math::Dot(isect.geom.sn, -ray.d))));
                        }
                    }
                    else
                    {
                        film->SetPixel(x, y, SPD(B(patchindex)));
                    }
                });
            }

            if (y % 10 == 0)
            {
                const double progress = 100.0 * y / film->Height();
                LM_LOG_INPLACE(boost::str(boost::format("Progress: %.1f%%") % progress));
            }
        }

        LM_LOG_INFO("Progress: 100.0%");

        #pragma endregion
    };

private:

    Float subdivLimitArea_;
    int wireframe_;
    int analyticalFormFactor_;

};

LM_COMPONENT_REGISTER_IMPL(Renderer_Radiosity, "renderer::radiosity");

LM_NAMESPACE_END
