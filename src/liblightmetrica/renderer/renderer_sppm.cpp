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

#define LM_SPPM_DEBUG 0
#define LM_SPPM_DEBUG_OUTPUT_PER_30_SEC 1
#define LM_SPPM_RENDER_WITH_TIME 1

/*!
    \brief Stochastic progressive photon mapping renderer.

    Implements stochastic progressive photon mapping [Hachisuka & Jensen 2009]
    References:
      - [Hachisuka & Jensen 2009] Stochastic progressive photon mapping
*/
class Renderer_SPPM final : public Renderer
{
public:

    LM_IMPL_CLASS(Renderer_SPPM, Renderer);

private:

    int maxNumVertices_;
    long long numIterationPass_;                          // Number of photon scattering passes
    long long numPhotonTraceSamples_;                     // Number of photon trace samples for each pass
    Float initialRadius_;                                 // Initial photon gather radius
    Float alpha_;                                         // Fraction to control photons (see paper)
    PhotonMap::UniquePtr photonmap_{ nullptr, nullptr };  // Underlying photon map implementation
    #if LM_SPPM_DEBUG
    std::string debugOutputPath_;
    #endif
    #if LM_SPPM_RENDER_WITH_TIME
    double renderTime_;
    #endif

public:

    LM_IMPL_F(Initialize) = [this](const PropertyNode* prop) -> bool
    {
        maxNumVertices_        = prop->Child("max_num_vertices")->As<int>();
        numIterationPass_      = prop->ChildAs<long long>("num_iteration_pass", 1000L);
        numPhotonTraceSamples_ = prop->ChildAs<long long>("num_photon_trace_samples", 100L);
        initialRadius_         = prop->ChildAs<Float>("initial_radius", 0.1_f);
        alpha_                 = prop->ChildAs<Float>("alpha", 0.7_f);
        photonmap_             = ComponentFactory::Create<PhotonMap>("photonmap::" + prop->ChildAs<std::string>("photonmap", "kdtree"));
        #if LM_SPPM_DEBUG
        debugOutputPath_       = prop->ChildAs<std::string>("debug_output_path", "sppm_%05d");
        #endif
        #if LM_SPPM_RENDER_WITH_TIME
        renderTime_            = prop->ChildAs("render_time", 10.0);
        #endif
        return true;
    };

    LM_IMPL_F(Render) = [this](const Scene* scene, Random* initRng, Film* film) -> void
    {
        #pragma region Render pass

        // Create measurement points shared with per pixel
        struct MeasurementPoint
        {
            bool valid;                     // True if the measurement point is valid
            Float radius;                   // Current photon radius
            Float N;                        // Accumulated photon count
            SPD tau;                        // Sum of throughput of luminance multiplies BSDF (Eq.10 in [Hachisuka et al. 2008]
            Vec3 wi;                        // Direction to previous vertex
            SPD throughputE;                // Throughput of importance
            PhotonMapUtils::PathVertex v;   // Current vertex information
            SPD emission;                   // Contribution of LS*E
            int numVertices;                // Number of vertices needed to generate the measurement point 
        };

        const auto W = film->Width();
        const auto H = film->Height();
        std::vector<MeasurementPoint> mps(W * H);
        for (auto& mp : mps)
        {
            mp.radius = initialRadius_;
            mp.N = 0_f;
        }

        long long totalPhotonTraceSamples = 0;

        #if LM_SPPM_RENDER_WITH_TIME
        const auto renderStartTime = std::chrono::high_resolution_clock::now();
        for (long long pass = 0; ; pass++)
        #else
        for (long long pass = 0; pass < numIterationPass_; pass++)
        #endif
        {
            LM_LOG_INFO("Pass " + std::to_string(pass));
            LM_LOG_INDENTER();
            
            // --------------------------------------------------------------------------------
            
            #pragma region Collect measumrement points
            {
                LM_LOG_INFO("Collect measurement points");
                LM_LOG_INDENTER();

                struct Context
                {
                    Random rng;
                };
                std::vector<Context> contexts(Parallel::GetNumThreads());
                for (auto& ctx : contexts)
                {
                    ctx.rng.SetSeed(initRng->NextUInt());
                }

                Parallel::For(W * H, [&](long long index, int threadid, bool init)
                {
                    auto& ctx = contexts[threadid];
                    const Vec2 initRasterPos(((Float)(index % W) + ctx.rng.Next()) / W, ((Float)(index / W) + ctx.rng.Next()) / H);
                    mps[index].valid = false;
                    PhotonMapUtils::TraceEyeSubpathFixedRasterPos(scene, &ctx.rng, maxNumVertices_, TransportDirection::EL, initRasterPos, [&](int numVertices, const Vec2& rasterPos, const PhotonMapUtils::PathVertex& pv, const PhotonMapUtils::PathVertex& v, const SPD& throughput) -> bool
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
                            auto& mp = mps[index];
                            mp.valid = true;
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
                                mp.emission += C;
                            }

                            return false;
                        }
                        return true;
                    });
                });
            }
            #pragma endregion

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
                    if (!mp.valid)
                    {
                        return;
                    }

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
                    mp.tau = (mp.tau + mp.throughputE * deltaTau) * ratio;
                    mp.radius = mp.radius * Math::Sqrt(ratio);
                    mp.N = mp.N + alpha_ * M;
                });

                // --------------------------------------------------------------------------------

                // Record to film
                film->Clear();
                for (int i = 0; i < (int)mps.size(); i++)
                {
                    const auto& mp = mps[i];
                    const auto C = mp.tau / (mp.radius * mp.radius * Math::Pi() * totalPhotonTraceSamples) + mp.emission / (Float)(pass + 1);
                    film->SetPixel(i % W, i / W, C);
                }
                #if LM_SPPM_DEBUG
                {
                    boost::format f(debugOutputPath_);
                    f.exceptions(boost::io::all_error_bits ^ (boost::io::too_many_args_bit | boost::io::too_few_args_bit));
                    film->Save(boost::str(f % pass));
                }
                #endif
                #if LM_SPPM_DEBUG_OUTPUT_PER_30_SEC
                {
                    static auto prevOutputTime = std::chrono::high_resolution_clock::now();
                    const auto currentTime = std::chrono::high_resolution_clock::now();
                    const double elapsed = (double)(std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - prevOutputTime).count()) / 1000.0;
                    if (elapsed > 30.0)
                    {
                        static long long outN = 0;
                        boost::format f("%03d");
                        f.exceptions(boost::io::all_error_bits ^ (boost::io::too_many_args_bit | boost::io::too_few_args_bit));
                        film->Save(boost::str(f % outN));
                        outN++;
                        prevOutputTime = currentTime;
                    }
                }
                #endif
            }
            #pragma endregion

            // --------------------------------------------------------------------------------

            #if LM_SPPM_RENDER_WITH_TIME
            {
                const auto currentTime = std::chrono::high_resolution_clock::now();
                const double elapsed = (double)(std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - renderStartTime).count()) / 1000.0;
                if (elapsed > renderTime_)
                {
                    break;
                }
            }
            #endif
        }
        #pragma endregion
    };

};

LM_COMPONENT_REGISTER_IMPL(Renderer_SPPM, "renderer::sppm");

LM_NAMESPACE_END
