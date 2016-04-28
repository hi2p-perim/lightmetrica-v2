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
#include <lightmetrica/scheduler.h>
#include <lightmetrica/scene.h>
#include <lightmetrica/primitive.h>
#include <lightmetrica/surfacegeometry.h>
#include <lightmetrica/ray.h>
#include <lightmetrica/intersection.h>
#include <lightmetrica/random.h>
#include <lightmetrica/light.h>
#include <lightmetrica/sensor.h>
#include <lightmetrica/film.h>
#include <lightmetrica/detail/photonmap.h>
#include <tbb/tbb.h>

LM_NAMESPACE_BEGIN

/*!
    \brief Photon mapping renderer.
    Implements photon mapping.
    References:
      - H. W. Jensen, Global illumination using photon maps,
        Procs. of the Eurographics Workshop on Rendering Techniques 96, pp.21-30, 1996.
      - H. W. Jensen, Realistic image synthesis using photon mapping,
        AK Peters, 2001
*/
class Renderer_PM final : public Renderer
{
public:

    LM_IMPL_CLASS(Renderer_PM, Renderer);

public:

    LM_IMPL_F(Initialize) = [this](const PropertyNode* prop) -> bool
    {
        sched_->Load(prop);
        maxNumVertices_ = prop->Child("max_num_vertices")->As<int>();
        numPhotonTraceSamples_ = prop->ChildAs<long long>("num_photon_trace_samples", 100000L);
        finalgather_ = prop->ChildAs<int>("finalgather", 1);
        pm_ = ComponentFactory::Create<PhotonMap>("photonmap::" + prop->ChildAs<std::string>("photonmap", "kdtree"));
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

        #pragma region Trace photons

        const auto photons = ProcessPhotonTrace([scene, this](Random* rng, std::vector<Photon>& photons)
        {
            #pragma region Sample a light

            const auto* L = scene->SampleEmitter(SurfaceInteractionType::L, rng->Next());
            const auto pdfL = scene->EvaluateEmitterPDF(L);
            assert(pdfL > 0_f);

            #pragma endregion

            // --------------------------------------------------------------------------------

            #pragma region Sample a position on the light and initial ray direction

            SurfaceGeometry geomL;
            Vec3 initWo;
            L->light->SamplePositionAndDirection(rng->Next2D(), rng->Next2D(), geomL, initWo);
            const auto pdfPL = L->light->EvaluatePositionGivenDirectionPDF(geomL, initWo, false);
            assert(pdfPL > 0_f);

            #pragma endregion

            // --------------------------------------------------------------------------------

            #pragma region Temporary variables

            auto throughput = L->light->EvaluatePosition(geomL, false) / pdfPL / pdfL;
            const auto* primitive = L;
            int type = SurfaceInteractionType::L;
            auto geom = geomL;
            Vec3 wi;
            int numVertices = 1;

            #pragma endregion

            // --------------------------------------------------------------------------------

            while (true)
            {
                if (maxNumVertices_ != -1 && numVertices >= maxNumVertices_)
                {
                    break;
                }

                // --------------------------------------------------------------------------------

                #pragma region Sample next direction

                Vec3 wo;
                if (type == SurfaceInteractionType::L)
                {
                    wo = initWo;
                }
                else
                {
                    primitive->surface->SampleDirection(rng->Next2D(), rng->Next(), type, geom, wi, wo);
                }
                const auto pdfD = primitive->surface->EvaluateDirectionPDF(geom, type, wi, wo, false);

                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Evaluate direction

                const auto fs = primitive->surface->EvaluateDirection(geom, type, wi, wo, TransportDirection::LE, false);
                if (fs.Black())
                {
                    break;
                }

                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Update throughput

                assert(pdfD > 0_f);
                throughput *= fs / pdfD;

                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Intersection

                // Setup next ray
                Ray ray = { geom.p, wo };

                // Intersection query
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

                #pragma region Record photon

                if ((isect.primitive->surface->Type() & SurfaceInteractionType::D) > 0 || (isect.primitive->surface->Type() & SurfaceInteractionType::G) > 0)
                {
                    Photon photon;
                    photon.p = isect.geom.p;
                    photon.throughput = throughput;
                    photon.wi = -ray.d;
                    photon.numVertices = numVertices + 1;
                    photons.push_back(photon);
                }

                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Path termination

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

                geom = isect.geom;
                primitive = isect.primitive;
                type = isect.primitive->surface->Type() & ~SurfaceInteractionType::Emitter;
                wi = -ray.d;
                numVertices++;

                #pragma endregion
            }
        });

        #pragma endregion

        // --------------------------------------------------------------------------------

        #pragma region Build photon map

        {
            LM_LOG_INFO("Building photon map");
            LM_LOG_INDENTER();
            pm_->Build(photons);
        }

        #pragma endregion

        // --------------------------------------------------------------------------------

        #pragma region Trace eye rays

        sched_->Process(scene, film, &initRng, [this](const Scene* scene, Film* film, Random* rng)
        {
            #pragma region Sample a sensor

            const auto* E = scene->SampleEmitter(SurfaceInteractionType::E, rng->Next());
            const auto pdfE = scene->EvaluateEmitterPDF(E);
            assert(pdfE.v > 0);

            #pragma endregion

            // --------------------------------------------------------------------------------

            #pragma region Sample a position on the sensor and initial ray direction

            SurfaceGeometry geomE;
            Vec3 initWo;
            E->sensor->SamplePositionAndDirection(rng->Next2D(), rng->Next2D(), geomE, initWo);
            const auto pdfPE = E->sensor->EvaluatePositionGivenDirectionPDF(geomE, initWo, false);
            assert(pdfPE.v > 0);

            #pragma endregion

            // --------------------------------------------------------------------------------

            #pragma region Calculate raster position for initial vertex

            Vec2 rasterPos;
            if (!E->sensor->RasterPosition(initWo, geomE, rasterPos))
            {
                // This can happen due to numerical errors
                return;
            }

            #pragma endregion

            // --------------------------------------------------------------------------------

            #pragma region Temporary variables

            auto throughput = E->sensor->EvaluatePosition(geomE, false) / pdfPE / pdfE;
            const auto* primitive = E;
            int type = SurfaceInteractionType::E;
            auto geom = geomE;
            Vec3 wi;
            int numVertices = 1;
            bool finalgather = !finalgather_;

            #pragma endregion

            // --------------------------------------------------------------------------------

            while (true)
            {
                if (maxNumVertices_ != -1 && numVertices >= maxNumVertices_)
                {
                    break;
                }
                
                // --------------------------------------------------------------------------------

                #pragma region Sample direction

                Vec3 wo;
                if (type == SurfaceInteractionType::E)
                {
                    wo = initWo;
                }
                else
                {
                    primitive->surface->SampleDirection(rng->Next2D(), rng->Next(), type, geom, wi, wo);
                }
                const auto pdfD = primitive->surface->EvaluateDirectionPDF(geom, type, wi, wo, false);

                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Evaluate direction

                const auto fs = primitive->surface->EvaluateDirection(geom, type, wi, wo, TransportDirection::EL, false);
                if (fs.Black())
                {
                    break;
                }

                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Update throughput

                assert(pdfD > 0_f);
                throughput *= fs / pdfD;

                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Intersection

                // Setup next ray
                Ray ray = { geom.p, wo };

                // Intersection query
                Intersection isect;
                if (!scene->Intersect(ray, isect))
                {
                    break;
                }

                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Handle hit with light source

                if ((isect.primitive->surface->Type() & SurfaceInteractionType::L) > 0)
                {
                    // Accumulate to film
                    const auto C =
                          throughput
                        * isect.primitive->emitter->EvaluateDirection(isect.geom, SurfaceInteractionType::L, Vec3(), -ray.d, TransportDirection::EL, false)
                        * isect.primitive->emitter->EvaluatePosition(isect.geom, false);
                    film->Splat(rasterPos, C);
                }

                if (isect.geom.infinite)
                {
                    break;
                }

                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Photon density estimation

                if ((isect.primitive->surface->Type() & SurfaceInteractionType::D) > 0 || (isect.primitive->surface->Type() & SurfaceInteractionType::G) > 0)
                {
                    if (finalgather)
                    {
                        #pragma region Collect photons

                        std::vector<Photon> collected;
                        const auto maxDist2 = pm_->CollectPhotons(isect.geom.p, 20, 0.1_f * 0.1_f, collected);

                        #pragma endregion

                        // --------------------------------------------------------------------------------

                        #pragma region Density estimation

                        const auto Kernel = [](const Vec3& p, const Photon& photon, Float maxDist2)
                        {
                            auto s = 1_f - Math::Length2(photon.p - p) / maxDist2;
                            return 3_f * Math::InvPi() * s * s;
                        };
                        for (const auto& photon : collected)
                        {
                            if (numVertices + photon.numVertices > maxNumVertices_)
                            {
                                continue;
                            }

                            auto k = Kernel(isect.geom.p, photon, maxDist2);
                            auto p = k / (maxDist2 * numPhotonTraceSamples_);
                            const auto f = isect.primitive->surface->EvaluateDirection(isect.geom, SurfaceInteractionType::BSDF, -ray.d, photon.wi, TransportDirection::EL, true);
                            const auto C = throughput * p * f * photon.throughput;
                            film->Splat(rasterPos, C);
                        }

                        #pragma endregion

                        // --------------------------------------------------------------------------------

                        // Stop the eye subpath if intersected vertex is not specular
                        if ((isect.primitive->surface->Type() & SurfaceInteractionType::S) == 0)
                        {
                            break;
                        }
                    }

                    finalgather = true;
                }

                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Path termination

                if (isect.geom.infinite)
                {
                    break;
                }

                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Update information

                geom = isect.geom;
                primitive = isect.primitive;
                type = isect.primitive->surface->Type() & ~SurfaceInteractionType::Emitter;
                wi = -ray.d;
                numVertices++;

                #pragma endregion
            }
        });

        #pragma endregion
    };

private:

    int maxNumVertices_;
    long long numPhotonTraceSamples_;
    int finalgather_;
    Scheduler::UniquePtr sched_ = ComponentFactory::Create<Scheduler>();
    PhotonMap::UniquePtr pm_{ nullptr, nullptr };

};

LM_COMPONENT_REGISTER_IMPL(Renderer_PM, "renderer::pm");

LM_NAMESPACE_END
