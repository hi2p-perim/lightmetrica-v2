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
#include <string>
#include <unordered_map>

LM_NAMESPACE_BEGIN

/*!
    String template.
   
    Implements string template supports.
    This feature offers advanced control on the scene configuration file.
    The language syntax is based on the `ctemplate` library:
    https://github.com/olafvdspek/ctemplate
    
    Although we leave the detailed explanation to the documentation of the ctemplate library,
    for convenience we briefly describe the usage of the template language.
    
    TODO.

    TODO. Create `internal` group in doxygen.
*/
class StringTemplate
{
public:

    LM_DISABLE_CONSTRUCT(StringTemplate);

public:

    /*!
        Expand template.
    */
    LM_PUBLIC_API static auto Expand(const std::string& input, const std::unordered_map<std::string, std::string>& dict) -> std::string;

};

LM_NAMESPACE_END
