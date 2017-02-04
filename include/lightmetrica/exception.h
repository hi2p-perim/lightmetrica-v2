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
extern "C"
{
    LM_PUBLIC_API auto SEHUtils_EnableStructuralException() -> void;
    LM_PUBLIC_API auto SEHUtils_DisableStructuralException() -> void;
}
//! \endcond

/*!
    \brief SEH utilility.

    Helps to handle structured exception handling (SEH) in the Windows environment.
    Some exceptions such as floating-point exception is handled via SEH.
    We need to register a translator function to catch SEH from user code.

    \ingroup core
*/
class SEHUtils
{
public:

    LM_DISABLE_CONSTRUCT(SEHUtils);

public:

    //! Enable SEH exception.
    static auto EnableStructuralException() -> void { LM_EXPORTED_F(SEHUtils_EnableStructuralException); }

    //! Disbale SEH exception.
    static auto DisableStructuralException() -> void { LM_EXPORTED_F(SEHUtils_DisableStructuralException); }

};

LM_NAMESPACE_END
