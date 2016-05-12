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
#include <tbb/tbb.h>

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
        tbb::parallel_for(tbb::blocked_range<long long>(0, numSamples, 1000), [&](const tbb::blocked_range<long long>& range) -> void
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
                if (ctx.processed > 1000)
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

};

std::unique_ptr<ParallelImpl> ParallelImpl::instance_;

// --------------------------------------------------------------------------------

auto Parallel::SetNumThreads(int numThreads) -> void { ParallelImpl::Instance()->SetNumThreads(numThreads); }
auto Parallel::GetNumThreads() -> int { return ParallelImpl::Instance()->GetNumThreads(); }
auto Parallel::For(long long numSamples, const std::function<void(long long index, int threadid, bool init)>& processFunc) -> void { ParallelImpl::Instance()->For(numSamples, processFunc); }

LM_NAMESPACE_END
