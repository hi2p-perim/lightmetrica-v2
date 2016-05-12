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

#pragma once

#include <lightmetrica/macros.h>
#include <functional>

LM_NAMESPACE_BEGIN

///! Parallelization utilities.
class Parallel
{
public:

    /*!
        \brief Set number of threads.
        
        Set number of threads utilized in the parallized functions by `Parallel::For`.
        If the given `numThreads` is not greater than zero, the number of threads is set to
        `(number of detected cores) - numThreads`.
    */
    LM_PUBLIC_API static auto SetNumThreads(int numThreads) -> void;

    ///! Get current number of threads
    LM_PUBLIC_API static auto GetNumThreads() -> int;

    /*!
        \brief Parallized for-loop.
        
        Parallizes a for-loop indexed with 0 to `numSamples` calling the function `processFunc`.
        The number of threads set by `SetNumThreads` function is used for this process.
        
        The `processFunc` function takes three parameters:
        `index` for the current index of the loop, `threadid` for the 0-indexed thread index,
        and `init` for specifying the initialization flag.
        The `init` flag turns `true` if the function is called
        only after the thread specified by `threadid` is initially created.     
    */
    LM_PUBLIC_API static auto For(long long numSamples, const std::function<void(long long index, int threadid, bool init)>& processFunc) -> void;

};

LM_NAMESPACE_END
