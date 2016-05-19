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
#include <lightmetrica/scheduler.h>
#include <lightmetrica/property.h>
#include <lightmetrica/logger.h>
#include <lightmetrica/film.h>
#include <lightmetrica/random.h>
#include <lightmetrica/detail/stringtemplate.h>
#include <lightmetrica/detail/parallel.h>
#include <tbb/tbb.h>

LM_NAMESPACE_BEGIN

class Scheduler_ final : public Scheduler
{
public:

    LM_IMPL_CLASS(Scheduler_, Scheduler);

public:

    LM_IMPL_F(Load) = [this](const PropertyNode* prop) -> void
    {
        #pragma region Load parameters

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
    };

    LM_IMPL_F(Process) = [this](const Scene* scene, Film* film, Random* initRng, const std::function<void(Film*, Random*)>& processSampleFunc) -> long long
    {
        tbb::task_scheduler_init init(Parallel::GetNumThreads());

        // --------------------------------------------------------------------------------

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
            tbb::parallel_for(tbb::blocked_range<long long>(0, NumSamples, grainSize_), [&](const tbb::blocked_range<long long>& range) -> void
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
                    // Process sampleprocessedSamples
                    processSampleFunc(ctx.film.get(), &ctx.rng);

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

        // --------------------------------------------------------------------------------

        return processedSamples;
    };

    LM_IMPL_F(GetNumSamples) = [this]() -> long long
    {
        return numSamples_;
    };

private:

    int numThreads_;
    long long grainSize_;
    long long progressUpdateInterval_;
    double progressImageUpdateInterval_;
    std::string progressImageUpdateFormat_;

    long long numSamples_;      //!< Number of samples
    double renderTime_;         //!< Render time

};

LM_COMPONENT_REGISTER_IMPL_DEFAULT(Scheduler_);

LM_NAMESPACE_END
