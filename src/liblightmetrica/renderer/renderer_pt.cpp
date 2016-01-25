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
#include <lightmetrica/logger.h>
#include <lightmetrica/property.h>
#include <lightmetrica/random.h>
#include <lightmetrica/scene.h>
#include <lightmetrica/film.h>
#include <lightmetrica/bsdf.h>
#include <lightmetrica/ray.h>
#include <lightmetrica/intersection.h>
#include <lightmetrica/emitter.h>
#include <lightmetrica/surfacegeometry.h>
#include <lightmetrica/detail/stringtemplate.h>
#include <tbb/tbb.h>

LM_NAMESPACE_BEGIN

class Scheduler
{
private:

    int numThreads_;
    long long grainSize_;
    long long progressUpdateInterval_;
    double progressImageUpdateInterval_;
    std::string progressImageUpdateFormat_;
    long long numSamples_;      //!< Number of samples
    double renderTime_;         //!< Render time

public:

    auto Load(const PropertyNode* prop) -> void
    {
        #pragma region Load parameters

        if (prop->Child("num_threads"))
        {
            numThreads_ = prop->Child("num_threads")->As<int>();
        }
        else
        {
            #if LM_DEBUG_MODE
            numThreads_ = 1;
            #else
            numThreads_ = 0;
            #endif
        }
        if (numThreads_ <= 0)
        {
            numThreads_ = static_cast<int>(std::thread::hardware_concurrency()) + numThreads_;
        }

        #if LM_DEBUG_MODE
        grainSize_ = prop->ChildAs<long long>("grain_size", 10);
        #else
        grainSize_ = prop->ChildAs<long long>("grain_size", 10000);
        #endif

        progressUpdateInterval_ = prop->ChildAs<long long>("progress_update_interval", 100000);
        progressImageUpdateInterval_ = prop->ChildAs<double>("progress_image_update_interval", -1);
        if (progressImageUpdateInterval_ > 0)
        {
            progressImageUpdateFormat_ = prop->ChildAs<std::string>("progress_image_update_format", "progress/{{count}}.png");
        }

        numSamples_ = prop->ChildAs<long long>("num_samples", 10000000L);
        renderTime_ = prop->ChildAs<double>("render_time", -1);

        #pragma endregion

        // --------------------------------------------------------------------------------

        #pragma region Print loaded parameters

        {
            LM_LOG_INFO("Loaded parameters");
            LM_LOG_INDENTER();
            LM_LOG_INFO("num_threads                    = " + std::to_string(numThreads_));
            LM_LOG_INFO("grain_size                     = " + std::to_string(grainSize_));
            LM_LOG_INFO("progress_update_interval       = " + std::to_string(progressUpdateInterval_));
            LM_LOG_INFO("progress_image_update_interval = " + std::to_string(progressImageUpdateInterval_));
            LM_LOG_INFO("progress_image_update_format   = " + progressImageUpdateFormat_);
            LM_LOG_INFO("num_samples                    = " + std::to_string(numSamples_));
            LM_LOG_INFO("render_time                    = " + std::to_string(renderTime_));
        }

        #pragma endregion
    }

public:

    auto Process(const Scene* scene, Film* film, Random* initRng, const std::function<void(const Scene*, Film*, Random*)>& processSampleFunc) const -> void
    {
        #pragma region Thread local storage

        struct Context
        {
            int id = -1;						        // Thread ID
            Random rng;							        // Thread-specific RNG
            Film::UniquePtr film{ nullptr, nullptr };	// Thread specific film
            long long processedSamples = 0;	        	// Temp for counting # of processed samples
        };

        tbb::enumerable_thread_specific<Context> contexts;
        std::mutex contextInitMutex;
        int currentThreadID = 0;

        #pragma endregion

        // --------------------------------------------------------------------------------

        #pragma region Render loop

        std::atomic<long long> processedSamples(0);
        long long progressImageCount = 0;
        const auto renderStartTime = std::chrono::high_resolution_clock::now();
        auto prevImageUpdateTime = renderStartTime;
        const long long NumSamples = renderTime_ < 0 ? numSamples_ : grainSize_ * 1000;

        while (true)
        {
            #pragma region Helper function

            const auto ProcessProgress = [&](Context& ctx) -> void
            {
                processedSamples += ctx.processedSamples;
                ctx.processedSamples = 0;

                if (renderTime_ < 0)
                {
                    if (ctx.id == 0)
                    {
                        const double progress = (double)(processedSamples) / numSamples_ * 100.0;
                        LM_LOG_INPLACE(boost::str(boost::format("Progress: %.1f%%") % progress));
                    }
                }
                else
                {
                    if (ctx.id == 0)
                    {
                        const auto currentTime = std::chrono::high_resolution_clock::now();
                        const double elapsed = (double)(std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - renderStartTime).count()) / 1000.0;
                        const double progress = elapsed / renderTime_ * 100.0;
                        LM_LOG_INPLACE(boost::str(boost::format("Progress: %.1f%% (%.1fs / %.1fs)") % progress % elapsed % renderTime_));
                    }
                }
            };

            #pragma endregion

            // --------------------------------------------------------------------------------

            #pragma region Parallel loop

            std::atomic<bool> done(false);
            tbb::parallel_for(tbb::blocked_range<long long>(0, numSamples_, grainSize_), [&](const tbb::blocked_range<long long>& range) -> void
            {
                if (done)
                {
                    return;
                }

                // --------------------------------------------------------------------------------

                #pragma region Thread local storage

                auto& ctx = contexts.local();
                if (ctx.id < 0)
                {
                    std::unique_lock<std::mutex> lock(contextInitMutex);
                    ctx.id = currentThreadID++;
                    ctx.rng.SetSeed(initRng->NextUInt());
                    ctx.film = ComponentFactory::Clone<Film>(film);
                }

                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Sample loop

                for (long long sample = range.begin(); sample != range.end(); sample++)
                {
                    // Process sample
                    processSampleFunc(scene, ctx.film.get(), &ctx.rng);

                    // Report progress
                    ctx.processedSamples++;
                    if (ctx.processedSamples > progressUpdateInterval_)
                    {
                        ProcessProgress(ctx);
                    }
                }

                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Check termination

                if (renderTime_ > 0)
                {
                    const auto currentTime = std::chrono::high_resolution_clock::now();
                    const double elapsed = (double)(std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - renderStartTime).count()) / 1000.0;
                    if (elapsed > renderTime_)
                    {
                        done = true;
                    }
                }

                #pragma endregion
            });

            #pragma endregion

            // --------------------------------------------------------------------------------

            #pragma region Add remaining processed samples

            for (auto& ctx : contexts)
            {
                ProcessProgress(ctx);
            }

            #pragma endregion

            // --------------------------------------------------------------------------------

            #pragma region Progress update of intermediate image

            if (progressImageUpdateInterval_ > 0)
            {
                const auto currentTime = std::chrono::high_resolution_clock::now();
                const double elapsed = (double)(std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - prevImageUpdateTime).count()) / 1000.0;
                if (elapsed > progressImageUpdateInterval_)
                {
                    // Gather film data
                    film->Clear();
                    contexts.combine_each([&](const Context& ctx)
                    {
                        film->Accumulate(ctx.film.get());
                    });

                    // Rescale
                    film->Rescale((Float)(film->Width() * film->Height()) / processedSamples);

                    // Output path
                    progressImageCount++;
                    std::string path;
                    {
                        std::unordered_map<std::string, std::string> dict;
                        dict["count"] = boost::str(boost::format("%010d") % progressImageCount);
                        path = StringTemplate::Expand(progressImageUpdateFormat_, dict);
                        if (path.empty())
                        {
                            path = progressImageUpdateFormat_;
                        }
                    }

                    // Save image
                    {
                        LM_LOG_INFO("Saving progress: ");
                        LM_LOG_INDENTER();
                        film->Save(path);
                    }

                    // Update time
                    prevImageUpdateTime = currentTime;
                }
            }

            #pragma endregion

            // --------------------------------------------------------------------------------

            #pragma region Exit condition

            if (renderTime_ < 0 || done)
            {
                break;
            }

            #pragma endregion
        }

        LM_LOG_INFO("Progress: 100.0%");
        LM_LOG_INFO(boost::str(boost::format("# of samples: %d") % processedSamples));

        #pragma endregion

        // --------------------------------------------------------------------------------

        #pragma region Gather film data

        // Gather film data
        film->Clear();
        contexts.combine_each([&](const Context& ctx)
        {
            film->Accumulate(ctx.film.get());
        });

        // Rescale
        film->Rescale((Float)(film->Width() * film->Height()) / processedSamples);

        #pragma endregion
    }

};

/*!
    Implementation of path tracing.
*/
class Renderer_PT final : public Renderer
{
public:

    LM_IMPL_CLASS(Renderer_PT, Renderer);

public:

    LM_IMPL_F(Initialize) = [this](const PropertyNode* prop) -> bool
    {
        sched_.Load(prop);
        maxNumVertices_ = prop->Child("max_num_vertices")->As<int>();
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

        sched_.Process(scene, film, &initRng, [this](const Scene* scene, Film* film, Random* rng)
        {
            #pragma region Sample a sensor

            const auto* E = scene->SampleEmitter(SurfaceInteraction::E, rng->Next());
            const Float pdfE = scene->EvaluateEmitterPDF(SurfaceInteraction::E);
            assert(pdfE > 0);

            #pragma endregion

            // --------------------------------------------------------------------------------

            #pragma region Sample a position on the sensor

            SurfaceGeometry geomE;
            E->SamplePosition(rng->Next2D(), geomE);
            const Float pdfPE = E->EvaluatePositionPDF(geomE, true);
            assert(pdfPE > 0);

            #pragma endregion

            // --------------------------------------------------------------------------------

            #pragma region Temporary variables

            auto throughput = E->EvaluatePosition(geomE, true) / pdfPE / pdfE;
            const GeneralizedBSDF* bsdf = E;
            int type = SurfaceInteraction::E;
            auto geom = geomE;
            Vec3 wi;
            int numVertices = 1;
            Vec2 rasterPos;

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
                bsdf->SampleDirection(rng->Next2D(), rng->Next(), type, geom, wi, wo);
                const double pdfD = prim->EvaluateDirectionPDF(geom, type, wi, wo, true);

                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Calculate raster position for initial vertex

                if (type == SurfaceInteraction::E)
                {
                    bool result = static_cast<const Emitter*>(bsdf)->RasterPosition(wo, geom, rasterPos);
                    assert(result);
                }

                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Evaluate direction

                const auto fs = bsdf->EvaluateDirection(geom, type, wi, wo, TransportDirection::EL, true);
                if (fs.Black())
                {
                    break;
                }

                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Update throughput

                assert(pdfD > 0);
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

                if ((isect.primitive-> .Prim->Type & PrimitiveType::L) > 0)
                {
                    // Accumulate to film
                    ctx.film[pixelIndex] +=
                        throughput
                        * isect.Prim->EvaluateDirection(isect.geom, PrimitiveType::L, glm::dvec3(), -ray.d, TransportDirection::EL, false)
                        * isect.Prim->EvaluatePosition(isect.geom, false);
                }

                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Path termination

                const Float rrProb = 0.5;
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
                prim = isect.Prim;
                type = isect.Prim->Type & ~PrimitiveType::Emitter;
                wi = -ray.d;
                numVertices++;

                #pragma endregion
            }
        });
    };

private:

    int maxNumVertices_;
    Scheduler sched_;

};

LM_COMPONENT_REGISTER_IMPL(Renderer_PT, "renderer::pt");

LM_NAMESPACE_END
