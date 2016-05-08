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
#include <thread>
#include <boost/format.hpp>

LM_NAMESPACE_BEGIN

/*!
    \brief Progressive radiosity renderer.

    Implements progressive radiosity algorithm [Cohen et al. 1988].
    Similar to `renderer::radiosity`, this implementation only supports
    the diffues BSDF (`bsdf::diffuse`) and the area light (`light::area`).
    
    References:
      - [Cohen et al. 1988] A progressive refinement approach to fast radiosity image generation
      - [Cohen & Wallace 1995] Radiosity and realistic image synthesis
      - [Willmott & Heckbert 1997] An empirical comparison of radiosity algorithms
*/
class Renderer_ProgressiveRadiosity final : public Renderer
{
public:

    LM_IMPL_CLASS(Renderer_ProgressiveRadiosity, Renderer);

public:

    LM_IMPL_F(Initialize) = [this](const PropertyNode* prop) -> bool
    {
        subdivLimitArea_ = prop->ChildAs<Float>("subdivlimitarea", 0.1_f);
        wireframe_ = prop->ChildAs<int>("wireframe", 0);
        numIterations_ = prop->ChildAs<long long>("num_iterations", 1000L);
        return true;
    };

    LM_IMPL_F(Render) = [this](const Scene* scene, Film* film) -> void
    {
        // Create patches
        Patches patches;
        patches.Create(scene, subdivLimitArea_);

        // --------------------------------------------------------------------------------

        #pragma region Solve radiosity equation 

        LM_LOG_INFO("Solving radiosity equation");

        const int N = patches.Size();
        std::vector<Vec3> S(N);        // Unshot radiosity
        std::vector<Vec3> B(N);        // Solution

        // Setup emission term
        for (int i = 0; i < N; i++)
        {
            const auto& patch = patches.At(i);
            const auto* light = patch.primitive->light;
            if (!light)
            {
                continue;
            }
            S[i] = B[i] = light->Emittance().ToRGB();
        }

        for (long long iteration = 0; iteration < numIterations_; iteration++)
        {
            // Pick the patch with maximum power
            Float maxPower = 0;
            int maxPowerIndex = 0;
            for (int i = 0; i < N; i++)
            {
                const auto& patch = patches.At(i);
                const auto power = Math::Luminance(S[i]) * patch.Area();
                if (power > maxPower)
                {
                    maxPower = power;
                    maxPowerIndex = i;
                }
            }
            
            // Shoot the radiosity from the patch
            auto radToShoot = S[maxPowerIndex];
            S[maxPowerIndex] = Vec3();
            #pragma omp parallel for schedule(dynamic, 1)
            for (int i = 0; i < N; i++)
            {
                if (i == maxPowerIndex) continue;
                const auto ff = RadiosityUtils::EstimateFormFactor(scene, patches.At(maxPowerIndex), patches.At(i));
                const auto R  = patches.At(i).primitive->bsdf->Reflectance().ToRGB();
                const auto deltaRad = radToShoot * ff * R;
                B[i] += deltaRad;
                S[i] += deltaRad;
            }

            if (iteration % 100 == 0)
            {
                const double progress = 100.0 * iteration / numIterations_;
                LM_LOG_INPLACE(boost::str(boost::format("Progress: %.1f%%") % progress));
            }
        }

        LM_LOG_INFO("Progress: 100.0%");

        #pragma endregion

        // --------------------------------------------------------------------------------

        #pragma region Rendering (ray casting)

        LM_LOG_INFO("Visualizing result");

        const int width  = film->Width();
        const int height = film->Height();
        #pragma omp parallel for schedule(dynamic, 1)
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
                        film->SetPixel(x, y, SPD(B[patchindex]));
                    }
                });
            }

            #pragma omp master
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
    long long numIterations_;

};

LM_COMPONENT_REGISTER_IMPL(Renderer_ProgressiveRadiosity, "renderer::progressiveradiosity");

LM_NAMESPACE_END
