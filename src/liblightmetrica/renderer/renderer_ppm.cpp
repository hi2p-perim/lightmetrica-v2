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
#include <lightmetrica/detail/parallel.h>
#include <tbb/tbb.h>

LM_NAMESPACE_BEGIN

#define LM_PPM_DEBUG 1

/*!
    \brief Progressive photon mapping renderer.

    Implements progressive photon mapping [Hachisuka et al. 2008]
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
    long long numIterationPass_;                          // Number of photon scattering passes
    long long numPhotonTraceSamples_;                     // Number of photon trace samples for each pass
    Float initialRadius_;                                 // Initial photon gather radius
    Float alpha_;                                         // Fraction to control photons (see paper)
    PhotonMap::UniquePtr photonmap_{ nullptr, nullptr };  // Underlying photon map implementation
    #if LM_PPM_DEBUG
    std::string debugOutputPath_;
    #endif

public:

    LM_IMPL_F(Initialize) = [this](const PropertyNode* prop) -> bool
    {
        maxNumVertices_        = prop->Child("max_num_vertices")->As<int>();
        numSamples_            = prop->ChildAs<long long>("num_samples", 100000L);
        numIterationPass_         = prop->ChildAs<long long>("num_iteration_pass", 1000L);
        numPhotonTraceSamples_ = prop->ChildAs<long long>("num_photon_trace_samples", 100L);
        initialRadius_         = prop->ChildAs<Float>("initial_radius", 0.1_f);
        alpha_                 = prop->ChildAs<Float>("alpha", 0.7_f);
        photonmap_             = ComponentFactory::Create<PhotonMap>("photonmap::" + prop->ChildAs<std::string>("photonmap", "kdtree"));
        #if LM_PPM_DEBUG
        debugOutputPath_       = prop->ChildAs<std::string>("debug_output_path", "sppm_%05d");
        #endif
        return true;
    };

    LM_IMPL_F(Render) = [this](const Scene* scene, Random* initRng, Film* film) -> void
    {
        #pragma region Collect measumrement points
        
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
        {
            LM_LOG_INFO("Collect measurement points");
            LM_LOG_INDENTER();

            struct Context
            {
                Random rng;
                std::vector<MeasurementPoint> mps;
            };
            std::vector<Context> contexts(Parallel::GetNumThreads());
            for (auto& ctx : contexts)
            {
                ctx.rng.SetSeed(initRng->NextUInt());
            }

            Parallel::For(numSamples_, [&](long long index, int threadid, bool init)
            {
                auto& ctx = contexts[threadid];
                PhotonMapUtils::TraceSubpath(scene, &ctx.rng, maxNumVertices_, TransportDirection::EL, [&](int numVertices, const Vec2& rasterPos, const PhotonMapUtils::PathVertex& pv, const PhotonMapUtils::PathVertex& v, const SPD& throughput) -> bool
                {
                    // Skip initial vertex
                    if (numVertices == 1)
                    {
                        return true;
                    }

                    // Record the measurement point and terminate the path if the surface is D or G.
                    // Otherwise, continue to trace the path.
                    if ((v.type & SurfaceInteractionType::D) > 0 || (v.type & SurfaceInteractionType::G) > 0)
                    {
                        MeasurementPoint mp;
                        mp.radius = initialRadius_;
                        mp.rasterPos = rasterPos;
                        mp.N = 0_f;
                        mp.wi = Math::Normalize(pv.geom.p - v.geom.p);
                        mp.throughputE = throughput;
                        mp.v = v;
                        mp.numVertices = numVertices;

                        // Handle hit with light source
                        if ((v.primitive->Type() & SurfaceInteractionType::L) > 0)
                        {
                            const auto C =
                                throughput
                                * v.primitive->EvaluateDirection(v.geom, SurfaceInteractionType::L, Vec3(), Math::Normalize(pv.geom.p - v.geom.p), TransportDirection::EL, false)
                                * v.primitive->EvaluatePosition(v.geom, false);
                            mp.emission = C;
                        }

                        ctx.mps.push_back(std::move(mp));
                        return false;
                    }
                    return true;
                });
            });

            for (auto& ctx : contexts)
            {
                mps.insert(mps.end(), ctx.mps.begin(), ctx.mps.end());
            }
        }

        // --------------------------------------------------------------------------------

        #pragma region Photon scattering pass
        long long totalPhotonTraceSamples = 0;
        for (long long pass = 0; pass < numIterationPass_; pass++)
        {
            LM_LOG_INFO("Pass " + std::to_string(pass));
            LM_LOG_INDENTER();

            // --------------------------------------------------------------------------------

            #pragma region Trace photons
            std::vector<Photon> photons;
            {
                LM_LOG_INFO("Tracing photons");
                LM_LOG_INDENTER();
                
                struct Context
                {
                    Random rng;
                    std::vector<Photon> photons;
                };
                std::vector<Context> contexts(Parallel::GetNumThreads());
                for (auto& ctx : contexts)
                {
                    ctx.rng.SetSeed(initRng->NextUInt());
                }

                Parallel::For(numPhotonTraceSamples_, [&](long long index, int threadid, bool init)
                {
                    auto& ctx = contexts[threadid];
                    PhotonMapUtils::TraceSubpath(scene, &ctx.rng, maxNumVertices_, TransportDirection::LE, [&](int numVertices, const Vec2& /*rasterPos*/, const PhotonMapUtils::PathVertex& pv, const PhotonMapUtils::PathVertex& v, SPD& throughput) -> bool
                    {
                        // Skip initial vertex
                        if (numVertices == 1)
                        {
                            return true;
                        }

                        // Record photon
                        if ((v.type & SurfaceInteractionType::D) > 0 || (v.type & SurfaceInteractionType::G) > 0)
                        {
                            Photon photon;
                            photon.p = v.geom.p;
                            photon.throughput = throughput;
                            photon.wi = Math::Normalize(pv.geom.p - v.geom.p);
                            photon.numVertices = numVertices;
                            ctx.photons.push_back(photon);
                        }

                        // Path termination
                        const Float rrProb = 0.5_f;
                        if (ctx.rng.Next() > rrProb)
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

                for (auto& ctx : contexts)
                {
                    photons.insert(photons.end(), ctx.photons.begin(), ctx.photons.end());
                }

                totalPhotonTraceSamples += numPhotonTraceSamples_;
            }
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

            #pragma region Progressive density estimation
            {
                LM_LOG_INFO("Density estimation");
                LM_LOG_INDENTER();

                // --------------------------------------------------------------------------------

                Parallel::For(mps.size(), [&](long long index, int threadid, bool init)
                {
                    auto& mp = mps[index];

                    // Accumulate tau 
                    SPD deltaTau;
                    Float M = 0_f;
                    photonmap_->CollectPhotons(mp.v.geom.p, mp.radius, [&](const Photon& photon) -> void
                    {
                        if (mp.numVertices + photon.numVertices - 1 > maxNumVertices_)
                        {
                            return;
                        }
                        const auto f = mp.v.primitive->EvaluateDirection(mp.v.geom, SurfaceInteractionType::BSDF, mp.wi, photon.wi, TransportDirection::EL, true);
                        deltaTau += f * photon.throughput;
                        M += 1_f;
                    });

                    // Update information in the measreument point
                    if (mp.N + M == 0_f)
                    {
                        return;
                    }
                    const Float ratio = (mp.N + alpha_ * M) / (mp.N + M);
                    mp.tau = (mp.tau + deltaTau) * ratio;
                    mp.radius = mp.radius * Math::Sqrt(ratio);
                    mp.N = mp.N + alpha_ * M;
                });

                // --------------------------------------------------------------------------------

                // Record to film
                film->Clear();
                for (const auto& mp : mps)
                {
                    const auto p = 1_f / (mp.radius * mp.radius * Math::Pi() * totalPhotonTraceSamples);
                    const auto C = mp.throughputE * p * mp.tau + mp.emission;
                    film->Splat(mp.rasterPos, C);
                }
                film->Rescale((Float)(film->Width() * film->Height()) / numSamples_);
                #if LM_PPM_DEBUG
                {
                    boost::format f(debugOutputPath_);
                    f.exceptions(boost::io::all_error_bits ^ (boost::io::too_many_args_bit | boost::io::too_few_args_bit));
                    film->Save(boost::str(f % pass));
                }
                #endif
            }
            #pragma endregion
        }
        #pragma endregion
    };

};

LM_COMPONENT_REGISTER_IMPL(Renderer_PPM, "renderer::ppm");

LM_NAMESPACE_END
