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
extern "C" LM_PUBLIC_API auto FPUtils_EnableFPControl() -> bool;
extern "C" LM_PUBLIC_API auto FPUtils_DisableFPControl() -> bool;
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

    /*!
        \brief Enable floating point exceptions.
        \return `true` if succeed, otherwise `false`.
    */
    static auto EnableFPControl()  -> bool { return LM_EXPORTED_F(FPUtils_EnableFPControl); }

    /*!
        \brief Disables floating point exceptions.
        \return `true` if succeed, otherwise `false`.
    */
    static auto DisableFPControl() -> bool { return LM_EXPORTED_F(FPUtils_DisableFPControl); }

};

LM_NAMESPACE_END
