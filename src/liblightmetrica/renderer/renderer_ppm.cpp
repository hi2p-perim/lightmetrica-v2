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

    Implements progressive photon mapping
    References:
      - [Hachisuka et al. 2008] Progressive photon mapping
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
    Float initialRadius_;                                 // Initial photon gather radius
    Float alpha_;                                         // Fraction to control photons (see paper)
    PhotonMap::UniquePtr photonmap_{ nullptr, nullptr };  // Underlying photon map implementation

public:

    LM_IMPL_F(Initialize) = [this](const PropertyNode* prop) -> bool
    {
        maxNumVertices_        = prop->Child("max_num_vertices")->As<int>();
        numSamples_            = prop->ChildAs<long long>("num_samples", 100000L);
        numPhotonPass_         = prop->ChildAs<long long>("num_photon_pass", 1000L);
        numPhotonTraceSamples_ = prop->ChildAs<long long>("num_photon_trace_samples", 100L);
        initialRadius_         = prop->ChildAs<Float>("initial_radius", 0.1_f);
        alpha_                 = prop->ChildAs<Float>("alpha", 0.7_f);
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
        LM_LOG_INFO("Collect measurement points");

        struct MeasurementPoint
        {
            Float radius;                   // Current photon radius
            Vec2 rasterPos;                 // Raster position
            Float N;                        // Accumulated photon count
            Vec3 wi;                        // Direction to previous vertex
            SPD throughputE;                // Throughput of importance
            SPD tau;                        // Sum of throughput of luminance multiplies BSDF (Eq.10 in [Hachisuka et al. 2008]
            PhotonMapUtils::PathVertex v;   // Current vertex information
            SPD emission;                   // Contribution of LS*E
            int numVertices;                // Number of vertices needed to generate the measurement point 
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
                    mp.radius      = initialRadius_;
                    mp.rasterPos   = rasterPos;
                    mp.N           = 0_f;
                    mp.wi          = Math::Normalize(pv.geom.p - v.geom.p);
                    mp.throughputE = throughput;
                    mp.v           = v;
                    mp.numVertices = numVertices;

                    // Handle hit with light source
                    if ((v.primitive->surface->Type() & SurfaceInteractionType::L) > 0)
                    {
                        const auto C =
                            throughput
                            * v.primitive->emitter->EvaluateDirection(v.geom, SurfaceInteractionType::L, Vec3(), Math::Normalize(pv.geom.p - v.geom.p), TransportDirection::EL, false)
                            * v.primitive->emitter->EvaluatePosition(v.geom, false);
                        mp.emission = C;
                    }

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
        long long totalPhotonTraceSamples = 0;
        for (long long pass = 0; pass < numPhotonPass_; pass++)
        {
            LM_LOG_INFO("Pass " + std::to_string(pass));
            LM_LOG_INDENTER();

            // --------------------------------------------------------------------------------

            #pragma region Trace photons
            auto photons = PhotonMapUtils::ProcessPhotonTrace(&initRng, numPhotonTraceSamples_, [this, scene](Random* rng, std::vector<Photon>& photons)
            {
                PhotonMapUtils::TraceSubpath(scene, rng, maxNumVertices_, TransportDirection::LE, [&](int numVertices, const Vec2& /*rasterPos*/, const PhotonMapUtils::PathVertex& pv, const PhotonMapUtils::PathVertex& v, SPD& throughput) -> bool
                {
                    // Record photon
                    if ((v.type & SurfaceInteractionType::D) > 0 || (v.type & SurfaceInteractionType::G) > 0)
                    {
                        Photon photon;
                        photon.p = v.geom.p;
                        photon.throughput = throughput;
                        photon.wi = Math::Normalize(pv.geom.p - v.geom.p);
                        photon.numVertices = numVertices;
                        photons.push_back(photon);
                    }

                    // Path termination
                    const Float rrProb = 0.5_f;
                    if (rng->Next() > rrProb)
                    {
                        return false;
                    }
                    else
                    {
                        throughput /= rrProb;
                    }

                    return true;
                });
            });
            totalPhotonTraceSamples += numPhotonTraceSamples_;
            #pragma endregion

            // --------------------------------------------------------------------------------

            #pragma region Build photon map
            {
                LM_LOG_INFO("Building photon map");
                LM_LOG_INDENTER();
                photonmap_->Build(std::move(photons));
            }
            #pragma endregion
            
            // --------------------------------------------------------------------------------

            #pragma region Density estimation
            {
                LM_LOG_INFO("Density estimation");
                LM_LOG_INDENTER();

                std::atomic<size_t> processed(0);
                const auto mainThreadId = std::this_thread::get_id();
                tbb::parallel_for(tbb::blocked_range<size_t>(0, mps.size(), 1000), [&](const tbb::blocked_range<size_t>& range) -> void
                {
                    for (size_t i = range.begin(); i != range.end(); i++)
                    {
                        auto& mp = mps[i];

                        // Accumulate radiance
                        SPD deltaTau;
                        Float M = 0_f;
                        photonmap_->CollectPhotons(mp.v.geom.p, mp.radius, [&](const Photon& photon) -> void
                        {
                            if (mp.numVertices + photon.numVertices - 1 > maxNumVertices_)
                            {
                                return;
                            }
                            const auto f = mp.v.primitive->surface->EvaluateDirection(mp.v.geom, SurfaceInteractionType::BSDF, mp.wi, photon.wi, TransportDirection::EL, true);
                            deltaTau += f * photon.throughput;
                            M += 1_f;
                        });

                        // Progressive density estimation
                        if (mp.N + M == 0_f)
                        {
                            continue;
                        }
                        const Float ratio = (mp.N + alpha_ * M) / (mp.N + M);
                        mp.tau = (mp.tau + deltaTau) * ratio;
                        mp.radius = mp.radius * Math::Sqrt(ratio);
                        mp.N = mp.N + alpha_ * M;
                    }

                    // --------------------------------------------------------------------------------

                    processed += range.end() - range.begin();
                    if (std::this_thread::get_id() == mainThreadId && processed % 10000 == 0)
                    {
                        const double progress = (double)(processed) / mps.size() * 100.0;
                        LM_LOG_INPLACE(boost::str(boost::format("Progress: %.1f%%") % progress));
                    }
                });

                LM_LOG_INFO("Progress: 100.0%");
            }
            #pragma endregion
        }
        #pragma endregion

        // --------------------------------------------------------------------------------

        film->Clear();
        for (const auto& mp : mps)
        {
            const auto p = 1_f / (mp.radius * mp.radius * Math::Pi() * totalPhotonTraceSamples);
            const auto C = mp.throughputE * p * mp.tau + mp.emission;
            film->Splat(mp.rasterPos, C);
        }
        film->Rescale((Float)(film->Width() * film->Height()) / numSamples_);
    };

};

LM_COMPONENT_REGISTER_IMPL(Renderer_PPM, "renderer::ppm");

LM_NAMESPACE_END
