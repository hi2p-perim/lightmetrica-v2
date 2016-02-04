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

/*!
    Types.
*/
enum class Types
{
    Class,
};

/*!
    Type info.
    Implements simple run-time reflection.
*/
struct TypeInfo
{
    Types type;
    const char* name;
    struct
    {
        const char* base;
    } classT;
};

#define LM_DEFINE_CLASS_TYPE(ClassType, BaseClassType) \
    static TypeInfo Type_() { \
        TypeInfo t; \
        t.type = Types::Class; \
        t.name = #ClassType; \
        t.classT.base = #BaseClassType; \
        return t; \
    }

LM_NAMESPACE_END
