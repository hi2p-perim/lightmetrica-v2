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

#pragma region Vec3

/*!
    3D vector.
    
    Generic 3-dimensional vector.
*/
template <typename T, SIMD Opt = SIMD::None>
struct TVec3;

template <typename T>
struct TVec3<T, SIMD::None>
{

    using VT = T;
    using VecT = TVec3<T, SIMD::None>;
    static constexpr int NumComponents = 3;

    union
    {
        T v_[3];
        struct { T x, y, z; };
    };

    TVec3() : x(T(0)), y(T(0)), z(T(0)) {}
    TVec3(const T& x, const T& y, const T& z) : x(x), y(y), z(z) {}
    TVec3(const VecT& v) : x(v.x), y(v.y), z(v.z) {}

    T& operator[](int i) { return (&x)[i]; }
    const T& operator[](int i) const { return (&x)[i]; }
    VecT& operator=(const VecT& v) { x = v.x; y = v.y; z = v.z; return *this; }

};

template <>
struct LM_ALIGN_16 TVec3<float, SIMD::SSE>
{

    using VT = float;
    using SIMDT = __m128;
    using VecT = TVec3<float, SIMD::SSE>;
    static constexpr int NumComponents = 3;

    union
    {
        SIMDT v_;
        struct { VT x, y, z, _; };
    };

    TVec3() : v_(_mm_set_ps(1.0f, 0.0f, 0.0f, 0.0f)) {}
    TVec3(VT x, VT y, VT z) : v_(_mm_set_ps(1.0f, z, y, x)) {}
    TVec3(const VecT& v) : v_(v.v_) {}
    TVec3(SIMDT v) : v_(v) {}

    VT& operator[](int i) { return (&x)[i]; }
    VT operator[](int i) const { return (&x)[i]; }
    VecT& operator=(const VecT& v) { v_ = v.v_; return *this; }

};

template <>
struct LM_ALIGN_32 TVec3<double, SIMD::AVX>
{

    using VT = double;
    using SIMDT = __m256d;
    using VecT = TVec3<double, SIMD::AVX>;
    static constexpr int NumComponents = 3;

    union
    {
        SIMDT v_;
        struct { VT x, y, z, _; };
    };

    TVec3() : v_(_mm256_set_pd(1, 0, 0, 0)) {}
    TVec3(VT x, VT y, VT z) : v_(_mm256_set_pd(1, z, y, x)) {}
    TVec3(const VecT& v) : v_(v.v_) {}
    TVec3(SIMDT v) : v_(v) {}

    VT& operator[](int i) { return (&x)[i]; }
    VT operator[](int i) const { return (&x)[i]; }
    VecT& operator=(const VecT& v) { v_ = v.v_; return *this; }

};

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Vec4

/*!
    4D vector.
    
    Generic 4-dimensional vector.
*/
template <typename T, SIMD Opt = SIMD::None>
struct TVec4;

template <typename T>
struct TVec4<T, SIMD::None>
{

    using VecT = TVec4<T, SIMD::None>;
    static constexpr int NumComponents = 4;

    union
    {
        T v_[3];
        struct { T x, y, z, w; };
    };

    TVec4() : x(T(0)), y(T(0)), z(T(0)), w(T(0)) {}
    TVec4(const T& x, const T& y, const T& z, const T& w) : x(x), y(y), z(z), w(w) {}
    TVec4(const VecT& v) : x(v.x), y(v.y), z(v.z), w(v.w) {}

    T& operator[](int i) { return (&x)[i]; }
    const T& operator[](int i) const { return (&x)[i]; }
    VecT& operator=(const VecT& v) { x = v.x; y = v.y; z = v.z; w = v.w; return *this; }

};

template <>
struct LM_ALIGN_16 TVec4<float, SIMD::SSE>
{

    using VT = float;
    using SIMDT = __m128;
    using VecT = TVec4<float, SIMD::SSE>;
    static constexpr int NumComponents = 4;

    union
    {
        SIMDT v_;
        struct { VT x, y, z, w; };
    };

    TVec4() : v_(_mm_set_ps(0.0f, 0.0f, 0.0f, 0.0f)) {}
    TVec4(VT x, VT y, VT z, VT w) : v_(_mm_set_ps(w, z, y, x)) {}
    TVec4(const VecT& v) : v_(v.v_) {}
    TVec4(SIMDT v) : v_(v) {}

    VT& operator[](int i) { return (&x)[i]; }
    VT operator[](int i) const { return (&x)[i]; }
    VecT& operator=(const VecT& v) { v_ = v.v_; return *this; }

};

template <>
struct LM_ALIGN_32 TVec4<double, SIMD::AVX>
{

    using VT = double;
    using SIMDT = __m256d;
    using VecT = TVec4<double, SIMD::AVX>;
    static constexpr int NumComponents = 4;

    union
    {
        SIMDT v_;
        struct { VT x, y, z, w; };
    };

    TVec4() : v_(_mm256_set_pd(0, 0, 0, 0)) {}
    TVec4(VT x, VT y, VT z, VT w) : v_(_mm256_set_pd(w, z, y, x)) {}
    TVec4(const VecT& v) : v_(v.v_) {}
    TVec4(SIMDT v) : v_(v) {}

    VT& operator[](int i) { return (&x)[i]; }
    VT operator[](int i) const { return (&x)[i]; }
    VecT& operator=(const VecT& v) { v_ = v.v_; return *this; }

};

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Some aliases

template <typename T, template <typename, SIMD> class VecT>
using VecTNone = VecT<T, SIMD::None>;

template <template <typename, SIMD> class VecT>
using VecTSSE = VecT<float, SIMD::SSE>;

template <template <typename, SIMD> class VecT>
using VecTAVX = VecT<double, SIMD::AVX>;

template <template <typename, SIMD> class VecT>
using EnableIfSSEType =
    typename std::enable_if_t<
        std::is_same<VecT<float, SIMD::SSE>, TVec3<float, SIMD::SSE>>::value ||
        std::is_same<VecT<float, SIMD::SSE>, TVec4<float, SIMD::SSE>>::value
    >;

template <template <typename, SIMD> class VecT>
using EnableIfAVXType =
    typename std::enable_if_t<
        std::is_same<VecT<double, SIMD::AVX>, TVec3<double, SIMD::AVX>>::value ||
        std::is_same<VecT<double, SIMD::AVX>, TVec4<double, SIMD::AVX>>::value
    >;

#pragma endregion

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Vector operations

#pragma region operator+

template <
    typename T,
    template <typename, SIMD> class VecT
>
auto operator+(const VecTNone<T, VecT>& v1, const VecTNone<T, VecT>& v2) -> VecTNone<T, VecT>
{
    constexpr int N = VecTNone<T, VecT>::NumComponents;
    VecTNone<T, VecT> result;
    for (int i = 0; i < N; i++) result[i] = v1[i] + v2[i];
    return result;
}

template <
    template <typename, SIMD> class VecT,
    typename = EnableIfSSEType<VecT>
>
auto operator+(const VecTSSE<VecT>& v1, const VecTSSE<VecT>& v2) -> VecTSSE<VecT>
{
    return VecTSSE<VecT>(_mm_add_ps(v1.v_, v2.v_));
}

template <
    template <typename, SIMD> class VecT,
    typename = EnableIfAVXType<VecT>
>
auto operator+(const VecTAVX<VecT>& v1, const VecTAVX<VecT>& v2) -> VecTAVX<VecT>
{
    return VecTAVX<VecT>(_mm256_add_pd(v1.v_, v2.v_));
}

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region operator-

template <
    typename T,
    template <typename, SIMD> class VecT
>
auto operator-(const VecTNone<T, VecT>& v1, const VecTNone<T, VecT>& v2) -> VecTNone<T, VecT>
{
    constexpr int N = VecTNone<T, VecT>::NumComponents;
    VecTNone<T, VecT> result;
    for (int i = 0; i < N; i++) result[i] = v1[i] - v2[i];
    return result;
}

template <
    template <typename, SIMD> class VecT,
    typename = EnableIfSSEType<VecT>
>
auto operator-(const VecTSSE<VecT>& v1, const VecTSSE<VecT>& v2) -> VecTSSE<VecT>
{
    return VecTSSE<VecT>(_mm_sub_ps(v1.v_, v2.v_));
}

template <
    template <typename, SIMD> class VecT,
    typename = EnableIfAVXType<VecT>
>
auto operator-(const VecTAVX<VecT>& v1, const VecTAVX<VecT>& v2) -> VecTAVX<VecT>
{
    return VecTAVX<VecT>(_mm256_sub_pd(v1.v_, v2.v_));
}

#pragma endregion

// --------------------------------------------------------------------------------



#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Matrix types


#pragma endregion

// --------------------------------------------------------------------------------

LM_NAMESPACE_END
