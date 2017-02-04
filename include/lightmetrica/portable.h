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
#include <vector>
#include <string>
#include <cstring>

LM_NAMESPACE_BEGIN

/*!
    \addtogroup component
    \{
*/

// Most default types fallen here
template <typename T>
struct Portable
{
    T v;
    Portable() {}
    Portable(T v) : v(v) {}
    auto Set(T result) -> void { v = result; }
    auto Get() -> T const { return v; }
};

template <>
struct Portable<void>
{
    Portable() {}
    auto Get() -> void const {}
};

template <typename T>
struct Portable<T&>
{
    T* v = nullptr;
    Portable(T& v) : v(&v) {}
    auto Set(T& result) const -> void { v = &result; }
    auto Get() const -> T& { return *v; }
};

// Specialization for unportable types
template <typename ContainerT>
struct Portable<std::vector<ContainerT>>
{
    using VectorT = std::vector<ContainerT>;
    using Type = VectorT;

    size_t N;
    ContainerT* p = nullptr;

    Portable() {}
    Portable(VectorT& v)
        : N(v.size())
        , p(&v[0])
    {}

    auto Get() const -> VectorT
    {
        VectorT v;
        v.assign(p, p + N);
        return std::move(v);
    }
};

// TODO: an observed bug with return value with std::string beyond dll boundary
template <>
struct Portable<std::string>
{
    char* p = nullptr;
    Portable() {}
    Portable(const std::string& s) { Set(s); }
    ~Portable() { LM_SAFE_DELETE_ARRAY(p); }
    auto Set(const std::string& s) -> void
    {
        LM_SAFE_DELETE_ARRAY(p);
        p = new char[s.size() + 1];
        memcpy(p, s.c_str(), s.size() + 1);
    }
    auto Get() const -> std::string { return std::string(p); }
};

template <>
struct Portable<const std::string&>
{
    const char* p = nullptr;
    Portable() {}
    Portable(const std::string& s) : p(s.c_str()) {}
    auto Set(const std::string& s) -> void { p = s.c_str(); }
    auto Get() const -> std::string { return std::string(p); }
};

//! \}

LM_NAMESPACE_END
