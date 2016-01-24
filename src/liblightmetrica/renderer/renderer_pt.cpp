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
#include <tbb/tbb.h>

LM_NAMESPACE_BEGIN

struct Scheduler
{
public:



    long long numSamples_;      //!< Number of samples
    double renderTime_;         //!< Render time

public:

    auto Load(const PropertyNode* prop) -> void
    {
        sched_.numSamples_ = prop->Child("num_samples")->As<long long>();
        sched_.renderTime_ = prop->Child("render_time")->As<double>();
    }

public:

    using ProcessSampleFuncType = std::function<void(const Scene&, Context&)>;
    auto Process(const Scene& scene, Random& initRng, std::vector<glm::dvec3>& film, const ProcessSampleFuncType& processSampleFunc) const -> void
    {
        #pragma region Thread local storage

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
        const long long NumSamples = Params.RenderTime < 0 ? Params.NumSamples : GrainSize * 1000;

        while (true)
        {
            #pragma region Helper function

            const auto ProcessProgress = [&](Context& ctx) -> void
            {
                processedSamples += ctx.processedSamples;
                ctx.processedSamples = 0;

                if (Params.RenderTime < 0)
                {
                    if (ctx.id == 0)
                    {
                        const double progress = (double)(processedSamples) / Params.NumSamples * 100.0;
                        NGI_LOG_INPLACE(boost::str(boost::format("Progress: %.1f%%") % progress));
                    }
                }
                else
                {
                    if (ctx.id == 0)
                    {
                        const auto currentTime = std::chrono::high_resolution_clock::now();
                        const double elapsed = (double)(std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - renderStartTime).count()) / 1000.0;
                        const double progress = elapsed / Params.RenderTime * 100.0;
                        NGI_LOG_INPLACE(boost::str(boost::format("Progress: %.1f%% (%.1fs / %.1fs)") % progress % elapsed % Params.RenderTime));
                    }
                }
            };

            #pragma endregion

            // --------------------------------------------------------------------------------

            #pragma region Parallel loop

            std::atomic<bool> done(false);
            tbb::parallel_for(tbb::blocked_range<long long>(0, NumSamples, GrainSize), [&](const tbb::blocked_range<long long>& range) -> void
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
                    ctx.rng.SetSeed(initRng.NextUInt());
                    ctx.film.assign(Params.Width * Params.Height, glm::dvec3());
                }

                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Sample loop

                for (long long sample = range.begin(); sample != range.end(); sample++)
                {
                    // Process sample
                    processSampleFunc(scene, ctx);

                    // Report progress
                    ctx.processedSamples++;
                    if (ctx.processedSamples > ProgressUpdateInterval)
                    {
                        ProcessProgress(ctx);
                    }
                }

                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Check termination

                if (Params.RenderTime > 0)
                {
                    const auto currentTime = std::chrono::high_resolution_clock::now();
                    const double elapsed = (double)(std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - renderStartTime).count()) / 1000.0;
                    if (elapsed > Params.RenderTime)
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

            if (ProgressImageUpdateInterval > 0)
            {
                const auto currentTime = std::chrono::high_resolution_clock::now();
                const double elapsed = (double)(std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - prevImageUpdateTime).count()) / 1000.0;
                if (elapsed > ProgressImageUpdateInterval)
                {
                    // Gather film data
                    film.assign(Params.Width * Params.Height, glm::dvec3());
                    contexts.combine_each([&](const Context& ctx)
                    {
                        std::transform(film.begin(), film.end(), ctx.film.begin(), film.begin(), std::plus<glm::dvec3>());
                    });
                    for (auto& v : film)
                    {
                        v *= (double)(Params.Width * Params.Height) / processedSamples;
                    }

                    // Output path
                    progressImageCount++;
                    std::string path;
                    {
                        namespace ct = ctemplate;
                        ct::TemplateDictionary dict("dict");
                        dict["count"] = boost::str(boost::format("%010d") % progressImageCount);

                        std::string output;
                        auto* tpl = ct::Template::StringToTemplate(ProgressImageUpdateFormat, ct::DO_NOT_STRIP);
                        if (!tpl->Expand(&output, &dict))
                        {
                            NGI_LOG_ERROR("Failed to expand template");
                            path = ProgressImageUpdateFormat;
                        }
                        else
                        {
                            path = output;
                        }
                    }

                    // Save image
                    {
                        NGI_LOG_INFO("Saving progress: ");
                        NGI_LOG_INDENTER();
                        SaveImage(path, film, Params.Width, Params.Height);
                    }

                    // Update time
                    prevImageUpdateTime = currentTime;
                }
            }

            #pragma endregion

            // --------------------------------------------------------------------------------

            #pragma region Exit condition

            if (Params.RenderTime < 0 || done)
            {
                break;
            }

            #pragma endregion
        }

        NGI_LOG_INFO("Progress: 100.0%");
        NGI_LOG_INFO(boost::str(boost::format("# of samples: %d") % processedSamples));

        #pragma endregion

        // --------------------------------------------------------------------------------

        #pragma region Gather film data

        film.assign(Params.Width * Params.Height, glm::dvec3());
        contexts.combine_each([&](const Context& ctx)
        {
            std::transform(film.begin(), film.end(), ctx.film.begin(), film.begin(), std::plus<glm::dvec3>());
        });
        for (auto& v : film)
        {
            v *= (double)(Params.Width * Params.Height) / processedSamples;
        }

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



        //const int w = film->Width();
        //const int h = film->Height();

        //for (int y = 0; y < h; y++)
        //{
        //    for (int x = 0; x < w; x++)
        //    {
        //        // Raster position
        //        Vec2 rasterPos((Float(x) + 0.5_f) / Float(w), (Float(y) + 0.5_f) / Float(h));

        //        // Position and direction of a ray
        //        const auto* E = scene->Sensor()->emitter;
        //        SurfaceGeometry geomE;
        //        E->SamplePosition(Vec2(), geomE);
        //        Vec3 wo;
        //        E->SampleDirection(rasterPos, 0_f, 0, geomE, Vec3(), wo);

        //        // Setup a ray
        //        Ray ray = { geomE.p, wo };

        //        // Intersection query
        //        Intersection isect;
        //        if (!scene->Intersect(ray, isect))
        //        {
        //            // No intersection -> black
        //            film->SetPixel(x, y, SPD());
        //            continue;
        //        }

        //        // Set color to the pixel
        //        const auto c = Math::Abs(Math::Dot(isect.geom.sn, -ray.d));
        //        film->SetPixel(x, y, SPD(c));
        //    }

        //    const double progress = 100.0 * y / film->Height();
        //    LM_LOG_INPLACE(boost::str(boost::format("Progress: %.1f%%") % progress));
        //}

        //LM_LOG_INFO("Progress: 100.0%");
    };

private:

    int maxNumVertices_;
    Scheduler sched_;

};

LM_COMPONENT_REGISTER_IMPL(Renderer_PT, "renderer::pt");

LM_NAMESPACE_END
