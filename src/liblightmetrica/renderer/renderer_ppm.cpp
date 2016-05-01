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
#include <lightmetrica/renderer.h>
#include <lightmetrica/property.h>
#include <lightmetrica/random.h>
#include <lightmetrica/scene.h>
#include <lightmetrica/film.h>
#include <lightmetrica/bsdf.h>
#include <lightmetrica/ray.h>
#include <lightmetrica/intersection.h>
#include <lightmetrica/emitter.h>
#include <lightmetrica/sensor.h>
#include <lightmetrica/surfacegeometry.h>
#include <lightmetrica/primitive.h>
#include <lightmetrica/scheduler.h>
#include <lightmetrica/renderutils.h>
#include <lightmetrica/detail/photonmap.h>
#include <lightmetrica/detail/photonmaputils.h>
#include <tbb/tbb.h>

LM_NAMESPACE_BEGIN

#define LM_PPM_DEBUG 0

/*!
    \brief Progressive photon mapping renderer.

    - Implements progressive photon mapping
    - Based on the simplified implementation by Toshiya Hachisuka
      http://www.ci.i.u-tokyo.ac.jp/~hachisuka/smallppm_exp.cpp
*/
class Renderer_PPM final : public Renderer
{
public:

    LM_IMPL_CLASS(Renderer_PPM, Renderer);

private:

    int maxNumVertices_;
    long long numSamples_;                                // Number of measurement points
    long long numPhotonPass_;                             // Number of photon scattering passes
    long long numPhotonTraceSamples_;                     // Number of photon trace samples for each pass
    PhotonMap::UniquePtr photonmap_{ nullptr, nullptr };  // Underlying photon map implementation

public:

    LM_IMPL_F(Initialize) = [this](const PropertyNode* prop) -> bool
    {
        maxNumVertices_        = prop->Child("max_num_vertices")->As<int>();
        numSamples_            = prop->ChildAs<long long>("num_samples", 100000L);
        numPhotonPass_         = prop->ChildAs<long long>("num_photon_pass", 1000L);
        numPhotonTraceSamples_ = prop->ChildAs<long long>("num_photon_trace_samples", 100L);
        photonmap_             = ComponentFactory::Create<PhotonMap>("photonmap::" + prop->ChildAs<std::string>("photonmap", "kdtree"));
        return true;
    };

    LM_IMPL_F(Render) = [this](const Scene* scene, Film* film) -> void
    {
        Random initRng;
        #if LM_DEBUG_MODE
        initRng.SetSeed(1008556906);
        #else
        initRng.SetSeed(static_cast<unsigned int>(std::time(nullptr)));
        #endif

        // --------------------------------------------------------------------------------

        #pragma region Collect measumrement points

        struct MeasurementPoint
        {
            Float radius;
            Vec2 rasterPos;
            SPD throughput;
            int numVertices;
            PhotonMapUtils::PathVertex v;
        };

        std::vector<MeasurementPoint> mps;
        for (long long sample = 0; sample < numSamples_; sample++)
        {
            PhotonMapUtils::TraceSubpath(scene, &initRng, maxNumVertices_, TransportDirection::EL, [&](int numVertices, const Vec2& rasterPos, const PhotonMapUtils::PathVertex& pv, const PhotonMapUtils::PathVertex& v, const SPD& throughput) -> bool
            {
                // Record the measurement point and terminate the path if the surface is D or G.
                // Otherwise, continue to trace the path.
                if ((v.type & SurfaceInteractionType::D) > 0 || (v.type & SurfaceInteractionType::G) > 0)
                {
                    MeasurementPoint mp;
                    mp.radius = 0.1_f;              // TODO: Determine from the sensor size
                    mp.rasterPos = rasterPos;
                    mp.throughput = throughput;
                    mp.numVertices = numVertices;
                    mp.v = v;
                    mps.push_back(std::move(mp));
                    return false;
                }
                return true;
            });
        }
        
        #pragma endregion

        // --------------------------------------------------------------------------------

        #if LM_PPM_DEBUG
        // Output the measurement points to the file
        {
            std::ofstream ofs("mp.dat");
            for (const auto& v : mp)
            {
                const auto& p = v.geom.p;
                ofs << boost::str(boost::format("%.15f %.15f %.15f") % p.x % p.y % p.z) << std::endl;
            }
        }
        #endif
        
        // --------------------------------------------------------------------------------

        #pragma region Photon scattering pass

        for (long long pass = 0; pass < numPhotonPass_; pass++)
        {
            LM_LOG_INFO("Photon scattering pass: " + std::to_string(pass));

            // Trace photons
            auto photons = PhotonMapUtils::ProcessPhotonTrace(&initRng, numPhotonTraceSamples_, [this, scene](Random* rng, std::vector<Photon>& photons)
            {
                PhotonMapUtils::TraceSubpath(scene, rng, maxNumVertices_, TransportDirection::LE, [&](int numVertices, const Vec2& /*rasterPos*/, const PhotonMapUtils::PathVertex& pv, const PhotonMapUtils::PathVertex& v, const SPD& throughput) -> bool
                {
                    if ((v.type & SurfaceInteractionType::D) > 0 || (v.type & SurfaceInteractionType::G) > 0)
                    {
                        Photon photon;
                        photon.p = v.geom.p;
                        photon.throughput = throughput;
                        photon.wi = Math::Normalize(pv.geom.p - v.geom.p);
                        photon.numVertices = numVertices;
                        photons.push_back(photon);
                    }
                    return true;
                });
            });

            // Build photon map
            photonmap_->Build(std::move(photons));

            // Density estimation
            for (const auto& mp : mps)
            {
                photonmap_->CollectPhotons(mp.v.geom.p, mp.radius, [&](const Photon& photon) -> void
                {
                    if (mp.numVertices + photon.numVertices - 1 > maxNumVertices_)
                    {
                        return;
                    }



                });
            }
        }

        #pragma endregion

        // --------------------------------------------------------------------------------

        
    };

};

LM_COMPONENT_REGISTER_IMPL(Renderer_PPM, "renderer::ppm");

LM_NAMESPACE_END
