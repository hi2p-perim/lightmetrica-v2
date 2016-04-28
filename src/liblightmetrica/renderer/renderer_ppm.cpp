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
    long long numSamples_;              // Number of measurement points
    long long numPhotonPass_;           // Number of photon scattering passes
    long long numPhotonTraceSamples_;   // Number of photon trace samples for each pass
    PhotonMap::UniquePtr photonmap_;    // Underlying photon map implementation

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

        #pragma region Function to trace subpath

        struct PathVertex
        {
            int type;
            SurfaceGeometry geom;
            const Primitive* primitive = nullptr;
        };

        const auto TraceSubpath = [this, &scene](Random* rng, TransportDirection transDir, const std::function<bool(int step, const Vec2&, const PathVertex&, const PathVertex&, const SPD&)>& processPathVertexFunc) -> void
        {
            Vec3 initWo;
            PathVertex pv, ppv;
            SPD throughput;
            Vec2 rasterPos;
            for (int step = 0; maxNumVertices_ == -1 || step < maxNumVertices_; step++)
            {
                if (step == 0)
                {
                    #pragma region Sample initial vertex

                    PathVertex v;

                    // Sample an emitter
                    v.type = transDir == TransportDirection::LE ? SurfaceInteractionType::L : SurfaceInteractionType::E;
                    v.primitive = scene->SampleEmitter(v.type, rng->Next());

                    // Sample a position on the emitter and initial ray direction
                    v.primitive->emitter->SamplePositionAndDirection(rng->Next2D(), rng->Next2D(), v.geom, initWo);

                    // Initial throughput
                    throughput =
                        v.primitive->emitter->EvaluatePosition(v.geom, false) /
                        v.primitive->emitter->EvaluatePositionGivenDirectionPDF(v.geom, initWo, false) /
                        scene->EvaluateEmitterPDF(v.primitive);

                    // Raster position
                    if (transDir == TransportDirection::EL)
                    {
                        v.primitive->sensor->RasterPosition(initWo, v.geom, rasterPos);
                    }

                    // Add a vertex
                    pv = v;

                    #pragma endregion
                }
                else
                {
                    #pragma region Sample a vertex with PDF with BSDF

                    // Sample a next direction
                    Vec3 wi;
                    Vec3 wo;
                    if (step == 1)
                    {
                        wi = Vec3();
                        wo = initWo;
                    }
                    else
                    {
                        wi = Math::Normalize(ppv.geom.p - pv.geom.p);
                        pv.primitive->surface->SampleDirection(rng->Next2D(), rng->Next(), pv.type, pv.geom, wi, wo);
                    }

                    // Evaluate direction
                    const auto fs = pv.primitive->surface->EvaluateDirection(pv.geom, pv.type, wi, wo, transDir, false);
                    if (fs.Black())
                    {
                        break;
                    }
                    const auto pdfD = pv.primitive->surface->EvaluateDirectionPDF(pv.geom, pv.type, wi, wo, false);
                    assert(pdfD > 0_f);

                    // Update throughput
                    throughput *= fs / pdfD;

                    // Intersection query
                    Ray ray = { pv.geom.p, wo };
                    Intersection isect;
                    if (!scene->Intersect(ray, isect))
                    {
                        break;
                    }

                    if (isect.geom.infinite)
                    {
                        break;
                    }

                    #pragma endregion

                    // --------------------------------------------------------------------------------

                    #pragma region Process path vertex

                    PathVertex v;
                    v.geom = isect.geom;
                    v.primitive = isect.primitive;
                    v.type = isect.primitive->surface->Type() & ~SurfaceInteractionType::Emitter;
                    if (!processPathVertexFunc(step, rasterPos, pv, v, throughput))
                    {
                        break;
                    }

                    #pragma endregion

                    // --------------------------------------------------------------------------------

                    #pragma region Path termination

                    // TODO: replace it with efficient one
                    const Float rrProb = 0.5_f;
                    if (rng->Next() > rrProb)
                    {
                        break;
                    }
                    else
                    {
                        throughput /= rrProb;
                    }

                    #pragma endregion

                    // --------------------------------------------------------------------------------

                    #pragma region Update information

                    ppv = pv;
                    pv = v;

                    #pragma endregion
                }
            }
        };

        #pragma endregion

        // --------------------------------------------------------------------------------

        #pragma region Collect measumrement points

        struct MeasurementPoint
        {
            Vec2 rasterPos;
            SPD throughput;
            PathVertex v;
        };

        std::vector<MeasurementPoint> mps;
        for (long long sample = 0; sample < numSamples_; sample++)
        {
            TraceSubpath(&initRng, TransportDirection::EL, [&](int /*step*/, const Vec2& rasterPos, const PathVertex& /*pv*/, const PathVertex& v, const SPD& throughput) -> bool
            {
                // Record the measurement point and terminate the path if the surface is D or G.
                // Otherwise, continue to trace the path.
                if ((v.type & SurfaceInteractionType::D) > 0 || (v.type & SurfaceInteractionType::G) > 0)
                {
                    MeasurementPoint mp;
                    mp.rasterPos = rasterPos;
                    mp.throughput = throughput;
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

        #pragma region Function to parallelize photon tracing

        const auto ProcessPhotonTrace = [&](const std::function<void(Random*, std::vector<Photon>&)>& processSampleFunc) -> std::vector<Photon>
        {
            LM_LOG_INFO("Tracing photons");
            LM_LOG_INDENTER();

            // --------------------------------------------------------------------------------

            #pragma region Thread local storage

            struct Context
            {
                std::thread::id id;
                Random rng;
                std::vector<Photon> photons;
                long long processedSamples = 0;
            };

            tbb::enumerable_thread_specific<Context> contexts;
            const auto mainThreadID = std::this_thread::get_id();
            std::mutex ctxInitMutex;
        
            #pragma endregion

            // --------------------------------------------------------------------------------

            #pragma region Render loop

            std::atomic<long long> processedSamples(0);
            tbb::parallel_for(tbb::blocked_range<long long>(0, numPhotonTraceSamples_, 10000), [&](const tbb::blocked_range<long long>& range) -> void
            {
                auto& ctx = contexts.local();
                if (ctx.id == std::thread::id())
                {
                    std::unique_lock<std::mutex> lock(ctxInitMutex);
                    ctx.id = std::this_thread::get_id();
                    ctx.rng.SetSeed(initRng.NextUInt());
                }

                for (long long sample = range.begin(); sample != range.end(); sample++)
                {
                    // Process sample
                    processSampleFunc(&ctx.rng, ctx.photons);

                    // Update progress
                    ctx.processedSamples++;
                    if (ctx.processedSamples > 100000)
                    {
                        processedSamples += ctx.processedSamples;
                        ctx.processedSamples = 0;
                        if (std::this_thread::get_id() == mainThreadID)
                        {
                            processedSamples += ctx.photons.size();
                            const double progress = (double)(processedSamples) / numPhotonTraceSamples_ * 100.0;
                            LM_LOG_INPLACE(boost::str(boost::format("Progress: %.1f%%") % progress));
                        }
                    }
                }
            });

            LM_LOG_INFO("Progress: 100.0%");

            #pragma endregion

            // --------------------------------------------------------------------------------

            #pragma region Gather results

            std::vector<Photon> photons;
            contexts.combine_each([&](const Context& ctx)
            {
                photons.insert(photons.end(), ctx.photons.begin(), ctx.photons.end());
            });

            #pragma endregion

            // --------------------------------------------------------------------------------

            LM_LOG_INFO(boost::str(boost::format("# of traced light paths: %d") % numPhotonTraceSamples_));
            LM_LOG_INFO(boost::str(boost::format("# of photons           : %d") % photons.size()));

            return photons;
        };

        #pragma endregion

        // --------------------------------------------------------------------------------

        #pragma region Photon scattering pass

        for (long long pass = 0; pass < numPhotonPass_; pass++)
        {
            LM_LOG_INFO("Photon scattering pass: " + std::to_string(pass));

            // Trace photons
            const auto photons = ProcessPhotonTrace([&](Random* rng, std::vector<Photon>& photons)
            {
                TraceSubpath(rng, TransportDirection::LE, [&](int step, const Vec2& /*rasterPos*/, const PathVertex& pv, const PathVertex& v, const SPD& throughput) -> bool
                {
                    if ((v.type & SurfaceInteractionType::D) > 0 || (v.type & SurfaceInteractionType::G) > 0)
                    {
                        Photon photon;
                        photon.p = v.geom.p;
                        photon.throughput = throughput;
                        photon.wi = Math::Normalize(pv.geom.p - pv.geom.p);
                        photon.numVertices = step + 2;
                        photons.push_back(photon);
                    }
                });
            });

            // Build photon map
            photonmap_->Build(photons);

            // Density estimation
            std::vector<Photon> collectedPhotons;
            for (const auto& mp : mps)
            {
                // Collect photons
                //photonmap_->CollectPhotons(mp.v.geom.p, )
            }
        }

        #pragma endregion

        // --------------------------------------------------------------------------------

        
    };

};

LM_COMPONENT_REGISTER_IMPL(Renderer_PPM, "renderer::ppm");

LM_NAMESPACE_END
