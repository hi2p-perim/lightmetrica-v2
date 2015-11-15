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

LM_NAMESPACE_BEGIN

/*
    Floating-point exception control.
    
    Controls floating-point exceptions.
    In this framework, the floating-point exception should be enabled by default.
    Some external libraies needs to explicitly disable floating-point exception control feature.
    In such a case, the target function call is wrapped by `EnableFPException` and `DisableFPException`.
    This feature is only supported with Visual Studio in Windows environment.
*/

extern "C"
{
    LM_PUBLIC_API auto 
}

class FloatintPointUtils
{
private:

    LM_DISABLE_CONSTRUCT(FloatintPointUtils);

public:

    static auto EnableFPControl() -> bool { return EnableFPControl_();  }
    static auto DisableFPControl() -> bool {}

private:

    using EnableFPControl_Type = bool(*)();
    EnableFPControl_Type EnableFPControl_;

};

LM_NAMESPACE_END
