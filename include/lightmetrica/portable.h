/*
    Lightmetrica - A modern, research-oriented renderer
    
    Copyright (c) 2015 Hisanari Otsu
    
    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files(the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions :
    
    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.
    
    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.
*/

#pragma once

#include <lightmetrica/macros.h>
#include <vector>
#include <string>

LM_NAMESPACE_BEGIN

// Most default types fallen here
template <typename T>
struct Portable
{
    T v;
    Portable<T>(T v) : v(v) {}
    T Get() const { return v; }
};

template <>
struct Portable<void>
{
    Portable<void>() {}
    void Get() const {}
};

template <typename T>
struct Portable<T&>
{
    T* v;
    Portable<T&>(T& v) : v(&v) {}
    T& Get() const { return *v; }
};

// Spefialization for unportable types
template <typename ContainerT>
struct Portable<std::vector<ContainerT>>
{
    using VectorT = std::vector<ContainerT>;
    using Type = VectorT;

    size_t N;
    ContainerT* p;

    Portable<VectorT>(VectorT& v)
        : N(v.size())
        , p(&v[0])
    {}

    VectorT Get() const
    {
        VectorT v;
        v.assign(p, p + N);
        return std::move(v);
    }
};

template <>
struct Portable<const std::string&>
{
    const char* p;

    Portable<const std::string&>(const std::string& s)
        : p(s.c_str())
    {}

    std::string Get() const
    {
        return std::string(p);
    }
};

LM_NAMESPACE_END
