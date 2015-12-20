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

// TODO. Make configurable from cmake file 
#define LM_USE_DOUBLE_PRECISION
#define LM_USE_AVX

// --------------------------------------------------------------------------------

#pragma region Precision mode

#ifdef LM_USE_SINGLE_PRECISION
	#define LM_SINGLE_PRECISION 1
#else
	#define LM_SINGLE_PRECISION 0
#endif
#ifdef LM_USE_DOUBLE_PRECISION
	#define LM_DOUBLE_PRECISION 1
#else
	#define LM_DOUBLE_PRECISION 0
#endif
#if LM_SINGLE_PRECISION + LM_DOUBLE_PRECISION != 1
	#error "Invalid precision mode"
#endif

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region SIMD support

#ifdef LM_USE_NO_SIMD
    #define LM_NO_SIMD 1
#else
    #define LM_NO_SIMD 0
#endif
#ifdef LM_USE_SSE
	#define LM_SSE 1
#else
	#define LM_SSE 0
#endif
#ifdef LM_USE_AVX
	#define LM_AVX 1
#else
	#define LM_AVX 0
#endif
#if LM_NO_SIMD + LM_SSE + LM_AVX != 1
	#error "Invalid SIMD support flag"
#endif

#if LM_AVX
#include <immintrin.h>  // Assume AVX2
#elif LM_SSE
#include <nmmintrin.h>  // Assume SSE4.2
#endif

#pragma endregion

// --------------------------------------------------------------------------------

LM_NAMESPACE_BEGIN

// --------------------------------------------------------------------------------

#pragma region Default floating point type

// Default type
#if LM_SINGLE_PRECISION
using Float = float;
#elif LM_DOUBLE_PRECISION
using Float = double;
#endif

// Convert to default floating point type
namespace
{
    auto operator"" _f(long double v) -> Float { return Float(v); }
    auto operator"" _f(unsigned long long v) -> Float { return Float(v); }
    auto operator"" _sf(const char* v) -> Float
    {
        #if LM_SINGLE_PRECISION
        return std::stof(v);
        #elif LM_DOUBLE_PRECISION
        return std::stod(v);
        #endif
    }
}

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region SIMD flag

/*!
    SIMD flags.

    Specified SIMD optimization, which is
    utilized as a template parameter.
*/
enum class SIMD
{
    None,
    SSE,    // Requires support of SSE, SSE2, SSE3, SSE4.x
    AVX,    // Requires support of AVX, AVX2
};

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Vector types

/*!
    3D vector.
    
    Generic 3-dimensional vector.
*/
template <typename T, SIMD Opt = SIMD::None>
struct TVec3;

template <typename T>
struct TVec3<T, SIMD::None>
{

    using ValueT = T;
    using VecT = TVec3<float, SIMD::None>;
    static constexpr int NumComponents = 3;

    union
    {
        T v_[3];
        struct { T x, y, z; };
    };

    TVec3() : x(T(0)), y(T(0)), z(T(0)) {}
    TVec3(const T& x, const T& y, const T& z) : x(x), y(y), z(z) {}

    T& operator[](int i) { return (&x)[i]; }
    const T& operator[](int i) const { return (&x)[i]; }

};

template <>
struct LM_ALIGN_16 TVec3<float, SIMD::SSE>
{

    using ValueT = float;
    using VecT = TVec3<float, SIMD::SSE>;
    static constexpr int NumComponents = 3;

    union
    {
        __m128 v_;
        struct { float x, y, z, _; };
    };

    TVec3() : v_(_mm_set_ps(1.0f, 0.0f, 0.0f, 0.0f)) {}
    TVec3(float x, float y, float z) : v_(_mm_set_ps(1.0f, z, y, x)) {}

    float& operator[](int i) { return (&x)[i]; }
    float operator[](int i) const { return (&x)[i]; }

};

template <>
struct LM_ALIGN_32 TVec3<double, SIMD::AVX>
{

    using ValueT = double;
    using VecT = TVec3<double, SIMD::AVX>;
    static constexpr int NumComponents = 3;

    union
    {
        __m256d v_;
        struct { double x, y, z, _; };
    };

    TVec3() : v_(_mm256_set_pd(1, 0, 0, 0)) {}
    TVec3(double x, double y, double z) : v_(_mm256_set_pd(1, z, y, x)) {}

    double& operator[](int i) { return (&x)[i]; }
    double operator[](int i) const { return (&x)[i]; }

};

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Vector operations



#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Matrix types


#pragma endregion

// --------------------------------------------------------------------------------

LM_NAMESPACE_END
