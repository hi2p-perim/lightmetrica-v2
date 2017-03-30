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
#include <lightmetrica/detail/parallel.h>
#include <lightmetrica/logger.h>
#define TBB_PREVIEW_LOCAL_OBSERVER 1
#include <tbb/tbb.h>
#include <tbb/task_scheduler_observer.h>

LM_NAMESPACE_BEGIN

class ParallelImpl
{
private:
    
    static std::unique_ptr<ParallelImpl> instance_;

public:

    static ParallelImpl* Instance()
    {
        if (!instance_) instance_.reset(new ParallelImpl);
        return instance_.get();
    }
    
private:

    #if LM_DEBUG_MODE
    int numThreads_ = 1;
    #else
    int numThreads_ = static_cast<int>(std::thread::hardware_concurrency());
    #endif

private:

    long long progressUpdateInterval_ = 1000;
    #if LM_DEBUG_MODE
    long long grainSize_ = 1000;
    #else
    long long grainSize_ = 10000;
    #endif

public:

    auto SetNumThreads(int numThreads)
    {
        numThreads_ = numThreads;
        if (numThreads_ <= 0)
        {
            numThreads_ = static_cast<int>(std::thread::hardware_concurrency()) + numThreads_;
        }
    }

    auto GetNumThreads() const -> int
    {
        return numThreads_;
    }

    auto For(long long numSamples, const std::function<void(long long index, int threadid, bool init)>& processFunc) const
    {
        tbb::task_scheduler_init tbbinit(numThreads_);
        const auto mainThreadId = std::this_thread::get_id();

        // --------------------------------------------------------------------------------

        struct Context
        {
            int threadid = -1;
            long long processed = 0;
        };
        tbb::enumerable_thread_specific<Context> contexts;
        std::mutex contextInitMutex;
        int currentThreadID = 0;
        
        // --------------------------------------------------------------------------------

        std::atomic<long long> processed(0);
        tbb::parallel_for(tbb::blocked_range<long long>(0, numSamples, grainSize_), [&](const tbb::blocked_range<long long>& range) -> void
        {
            bool init = false;
            auto& ctx = contexts.local();
            if (ctx.threadid < 0)
            {
                init = true;
                std::unique_lock<std::mutex> lock(contextInitMutex);
                ctx.threadid = currentThreadID++;
            }

            // --------------------------------------------------------------------------------

            for (long long i = range.begin(); i != range.end(); i++)
            {
                processFunc(i, ctx.threadid, init && i == range.begin());
                ctx.processed++;
                if (ctx.processed > progressUpdateInterval_)
                {
                    processed += ctx.processed;
                    ctx.processed = 0;
                    if (std::this_thread::get_id() == mainThreadId)
                    {
                        const double progress = (double)(processed) / numSamples * 100.0;
                        LM_LOG_INPLACE(boost::str(boost::format("Progress: %.1f%%") % progress));
                    }
                }
            }

        });

        LM_LOG_INFO("Progress: 100.0%");
    }

    auto For(const ParallelForParams& params, const std::function<void(long long index, int threadid, bool init)>& processFunc) -> long long
    {
        tbb::task_scheduler_init tbbinit(numThreads_);
        const auto mainThreadId = std::this_thread::get_id();

        // --------------------------------------------------------------------------------

        std::atomic<long long> processed(0);
        const auto startTime = std::chrono::high_resolution_clock::now();
        const auto ReportProgress = [&]() -> void
        {
            if (params.mode == ParallelMode::Samples)
            {
                const double progress = (double)(processed) / params.numSamples * 100.0;
                LM_LOG_INPLACE(boost::str(boost::format("Progress: %.1f%%") % progress));
            }
            else if (params.mode == ParallelMode::Time)
            {
                const auto currentTime = std::chrono::high_resolution_clock::now();
                const double elapsed = (double)(std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count()) / 1000.0;
                const double progress = elapsed / params.duration * 100.0;
                LM_LOG_INPLACE(boost::str(boost::format("Progress: %.1f%% (%.1fs / %.1fs)") % progress % elapsed % params.duration));
            }
            else
            {
                LM_UNREACHABLE();
            }
        };

        std::atomic<bool> done(false);
        struct Context
        {
            long long processed = 0;
        };
        std::vector<Context> contexts(numThreads_);
        do
        {
            #pragma region TLS
            contexts.assign(numThreads_, Context());
            #pragma endregion

            // --------------------------------------------------------------------------------

            #pragma region Parallel loop
            tbb::parallel_for(tbb::blocked_range<long long>(0, params.mode == ParallelMode::Samples ? params.numSamples : grainSize_ * 1000, grainSize_), [&](const tbb::blocked_range<long long>& range) -> void
            {
                if (done) { return; }

                // --------------------------------------------------------------------------------

                #pragma region TLS
                const int threadid = tbb::this_task_arena::current_thread_index();
                auto& ctx = contexts[threadid];
                #pragma endregion
                
                // --------------------------------------------------------------------------------

                #pragma region Sample loop
                for (long long i = range.begin(); i != range.end(); i++)
                {
                    // TODO. Fix it. Keeping thrid argument to false now.
                    processFunc(processed + i, threadid, false);
                    ctx.processed++;
                    if (ctx.processed > progressUpdateInterval_)
                    {
                        processed += ctx.processed;
                        ctx.processed = 0;
                        if (std::this_thread::get_id() == mainThreadId)
                        {
                            ReportProgress();
                        }
                    }
                }
                #pragma endregion
                
                // --------------------------------------------------------------------------------

                #pragma region Check termination
                if (params.mode == ParallelMode::Time)
                {
                    const auto currentTime = std::chrono::high_resolution_clock::now();
                    const double elapsed = (double)(std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count()) / 1000.0;
                    if (elapsed > params.duration)
                    {
                        done = true;
                    }
                }
                #pragma endregion
            });
            for (auto& ctx : contexts)
            {
                processed += ctx.processed;
                ctx.processed = 0;
            }
            ReportProgress();
            #pragma endregion

            // --------------------------------------------------------------------------------

            // TODO. Give chance to the caller to update something, e.g., intermediate images

        } while (params.mode == ParallelMode::Time && !done);

        // --------------------------------------------------------------------------------

        LM_LOG_INFO("Progress: 100.0%");
        {
            LM_LOG_INFO("Completed parallel process");
            LM_LOG_INDENTER();
            LM_LOG_INFO("Mode: " + std::string(params.mode == ParallelMode::Samples ? "Samples" : "Time"));
            LM_LOG_INFO(boost::str(boost::format("Processed # of samples: %d") % processed));
            if (params.mode == ParallelMode::Time)
            {
                const auto currentTime = std::chrono::high_resolution_clock::now();
                const double elapsed = (double)(std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count()) / 1000.0;
                LM_LOG_INFO(boost::str(boost::format("Elapsed: %.2f s") % elapsed));
            }
        }
        
        return processed;
    }

};

std::unique_ptr<ParallelImpl> ParallelImpl::instance_;

// --------------------------------------------------------------------------------

auto Parallel::SetNumThreads(int numThreads) -> void { ParallelImpl::Instance()->SetNumThreads(numThreads); }
auto Parallel::GetNumThreads() -> int { return ParallelImpl::Instance()->GetNumThreads(); }
auto Parallel::For(long long numSamples, const std::function<void(long long index, int threadid, bool init)>& processFunc) -> void { ParallelImpl::Instance()->For(numSamples, processFunc); }
auto Parallel::For(const ParallelForParams& params, const std::function<void(long long index, int threadid, bool init)>& processFunc) -> long long { return ParallelImpl::Instance()->For(params, processFunc); }

LM_NAMESPACE_END
