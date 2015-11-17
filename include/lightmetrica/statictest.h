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

extern "C"
{
    LM_PUBLIC_API auto StaticFuncTest_Func1() -> int;
    LM_PUBLIC_API auto StaticFuncTest_Func2(int v1, int v2) -> int;
}

namespace detail
{
    class StaticTest
    {
    private:

        LM_DISABLE_CONSTRUCT(StaticTest);

    public:

        static auto Func1() -> int { LM_EXPORTED_F(StaticFuncTest_Func1); }
        static auto Func2(int v1, int v2) -> int { LM_EXPORTED_F(StaticFuncTest_Func2, v1, v2); }

    };
}

LM_NAMESPACE_END
