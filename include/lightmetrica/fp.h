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

#include <lightmetrica/static.h>

LM_NAMESPACE_BEGIN

//! \cond
extern "C" LM_PUBLIC_API auto FPUtils_EnableFPControl() -> void;
extern "C" LM_PUBLIC_API auto FPUtils_DisableFPControl() -> void;
extern "C" LM_PUBLIC_API auto FPUtils_PushFPControl() -> void;
extern "C" LM_PUBLIC_API auto FPUtils_PopFPControl() -> void;
//! \endcond

/*
    \brief Floating point exception control.
    
    Controls floating point exceptions.
    In this framework, the floating-point exception should be enabled by default.
    Some external libraies needs to explicitly disable floating point exception control feature.
    In such a case, the target function call is wrapped by `EnableFPException` and `DisableFPException`.
    This feature is only supported with Visual Studio in Windows environment.

    All enabled floating point exception is translated into C++ exception (std::runtime_error).
    An alternative way to detect the floating point exception
    is to utilize `fetestexcept` function, 
    however it does needs explicit check of the exception flags.

    The enabled flag would be
      - Invalid operation
          + The operand is invalid, e.g., any operation involving in
            the signaling NaN generates an exception.
      - Division by zero
          + The floating point value is divided by zero.
    
    For the practical point of view,
    overflow, underflow, inexact, and denormalized (x86 only)
    exceptions are not caught.

    When the floating point exception is disabled
    each operation generates different results,
    e.g., invalid operation returns qNaN.

    Thread safety:
    Internally the functions utilizes _controlfp_s function
    and this function is reported to be thread-safe:
    https://social.msdn.microsoft.com/Forums/vstudio/en-US/c5f645fc-4f1e-4641-b968-2988ea4a19d9/problem-with-controlfp-and-thread-context-openmp?forum=vclanguage#4018a853-fe25-4499-ad7e-f4fe57a5cb83

    TODO:
      - GCC support
      - Linux environment support
      - Add some explanation why some exception is not caught

    \ingroup core
*/
class FPUtils
{
public:

    LM_DISABLE_CONSTRUCT(FPUtils);

public:

    ///! Enable floating point exceptions
    static auto EnableFPControl()  -> void { LM_EXPORTED_F(FPUtils_EnableFPControl); }

    ///! Disable floating point exceptions
    static auto DisableFPControl() -> void { LM_EXPORTED_F(FPUtils_DisableFPControl); }

    ///! Push the current state for floating point exceptions
    static auto PushFPControl() -> void { LM_EXPORTED_F(FPUtils_PushFPControl); }

    ///! Pop the previous state for floating point exceptions
    static auto PopFPControl() -> void { LM_EXPORTED_F(FPUtils_PopFPControl); }

};

LM_NAMESPACE_END
