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

#pragma region Base vector type

/*!
    Base of all vector types.

    Defines required types as vector types.
*/
template <typename T, SIMD Opt, template <typename, SIMD> class VecT_, int NC_>
struct TVecBase
{

    // Value type
    using VT = T;

    // Vector type
    using VecT = VecT_<T, Opt>;

    // Number of components
    static constexpr int NC = NC_;

    // Parameter types
    using ParamT  = std::conditional_t<std::is_fundamental<T>::value, T, const T&>;
    using RetType = ParamT;

};

template <typename T, SIMD Opt, template <typename, SIMD> class VecT_, int NC_>
struct SIMDTVecBase : public TVecBase<T, Opt, VecT_, NC_>
{
    // SIMD vector type
    using SIMDT = std::conditional_t<Opt == SIMD::AVX, __m256d, __m128>;
};

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Vec3

/*!
    3D vector.
    
    Generic 3-dimensional vector.
*/
template <typename T, SIMD Opt = SIMD::None>
struct TVec3;

template <typename T>
struct TVec3<T, SIMD::None> : public TVecBase<T, SIMD::None, TVec3, 3>
{
    union
    {
        VT v_[NC];
        struct { VT x, y, z; };
    };

    LM_INLINE TVec3()                                   : x(VT(0)), y(VT(0)), z(VT(0)) {}
    LM_INLINE TVec3(ParamT x, ParamT y, ParamT z)       : x(x), y(y), z(z) {}
    LM_INLINE TVec3(const VecT& v)                      : x(v.x), y(v.y), z(v.z) {}

    LM_INLINE auto operator[](int i)         -> VT&     { return (&x)[i]; }
    LM_INLINE auto operator[](int i) const   -> RetType { return (&x)[i]; }
    LM_INLINE auto operator=(const VecT& v)  -> VecT&   { x = v.x; y = v.y; z = v.z; return *this; }
    LM_INLINE auto operator+=(const VecT& v) -> VecT&   { x += v.x; y += v.y; z += v.z; return *this; }
    LM_INLINE auto operator-=(const VecT& v) -> VecT&   { x -= v.x; y -= v.y; z -= v.z; return *this; }
    LM_INLINE auto operator*=(const VecT& v) -> VecT&   { x *= v.x; y *= v.y; z *= v.z; return *this; }
    LM_INLINE auto operator/=(const VecT& v) -> VecT&   { x /= v.x; y /= v.y; z /= v.z; return *this; }

};

template <>
struct LM_ALIGN_16 TVec3<float, SIMD::SSE> : public SIMDTVecBase<float, SIMD::SSE, TVec3, 3>
{
    union
    {
        SIMDT v_;
        struct { VT x, y, z, _; };
    };

    LM_INLINE TVec3()                                   : v_(_mm_set_ps(1.0f, 0.0f, 0.0f, 0.0f)) {}
    LM_INLINE TVec3(ParamT x, ParamT y, ParamT z)       : v_(_mm_set_ps(1.0f, z, y, x)) {}
    LM_INLINE TVec3(const VecT& v)                      : v_(v.v_) {}
    LM_INLINE TVec3(SIMDT v)                            : v_(v) {}

    LM_INLINE auto operator[](int i)         -> VT&     { return (&x)[i]; }
    LM_INLINE auto operator[](int i) const   -> RetType { return (&x)[i]; }
    LM_INLINE auto operator=(const VecT& v)  -> VecT&   { v_ = v.v_; return *this; }
    LM_INLINE auto operator+=(const VecT& v) -> VecT&   { v_ = _mm_add_ps(v_, v.v_); return *this; }
    LM_INLINE auto operator-=(const VecT& v) -> VecT&   { v_ = _mm_sub_ps(v_, v.v_); return *this; }
    LM_INLINE auto operator*=(const VecT& v) -> VecT&   { v_ = _mm_mul_ps(v_, v.v_); return *this; }
    LM_INLINE auto operator/=(const VecT& v) -> VecT&   { v_ = _mm_div_ps(v_, v.v_); return *this; }

};

template <>
struct LM_ALIGN_32 TVec3<double, SIMD::AVX> : public SIMDTVecBase<double, SIMD::AVX, TVec3, 3>
{

    union
    {
        SIMDT v_;
        struct { VT x, y, z, _; };
    };

    LM_INLINE TVec3()                                   : v_(_mm256_set_pd(1, 0, 0, 0)) {}
    LM_INLINE TVec3(ParamT x, ParamT y, ParamT z)       : v_(_mm256_set_pd(1, z, y, x)) {}
    LM_INLINE TVec3(const VecT& v)                      : v_(v.v_) {}
    LM_INLINE TVec3(SIMDT v)                            : v_(v) {}

    LM_INLINE auto operator[](int i)         -> VT&     { return (&x)[i]; }
    LM_INLINE auto operator[](int i) const   -> RetType { return (&x)[i]; }
    LM_INLINE auto operator=(const VecT& v)  -> VecT&   { v_ = v.v_; return *this; }
    LM_INLINE auto operator+=(const VecT& v) -> VecT&   { v_ = _mm256_add_pd(v_, v.v_); return *this; }
    LM_INLINE auto operator-=(const VecT& v) -> VecT&   { v_ = _mm256_sub_pd(v_, v.v_); return *this; }
    LM_INLINE auto operator*=(const VecT& v) -> VecT&   { v_ = _mm256_mul_pd(v_, v.v_); return *this; }
    LM_INLINE auto operator/=(const VecT& v) -> VecT&   { v_ = _mm256_div_pd(v_, v.v_); return *this; }

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
struct TVec4<T, SIMD::None> : public TVecBase<T, SIMD::None, TVec4, 4>
{

    union
    {
        VT v_[NC];
        struct { VT x, y, z, w; };
    };

    LM_INLINE TVec4()                                       : x(VT(0)), y(VT(0)), z(VT(0)), w(VT(0)) {}
    LM_INLINE TVec4(ParamT x, ParamT y, ParamT z, ParamT w) : x(x), y(y), z(z), w(w) {}
    LM_INLINE TVec4(const VecT& v)                          : x(v.x), y(v.y), z(v.z), w(v.w) {}

    LM_INLINE auto operator[](int i)         -> VT&         { return (&x)[i]; }
    LM_INLINE auto operator[](int i) const   -> RetType     { return (&x)[i]; }
    LM_INLINE auto operator=(const VecT& v)  -> VecT&       { x = v.x; y = v.y; z = v.z; w = v.w; return *this; }
    LM_INLINE auto operator+=(const VecT& v) -> VecT&       { x += v.x; y += v.y; z += v.z; w += v.w; return *this; }
    LM_INLINE auto operator-=(const VecT& v) -> VecT&       { x -= v.x; y -= v.y; z -= v.z; w -= v.w; return *this; }
    LM_INLINE auto operator*=(const VecT& v) -> VecT&       { x *= v.x; y *= v.y; z *= v.z; w *= v.w; return *this; }
    LM_INLINE auto operator/=(const VecT& v) -> VecT&       { x /= v.x; y /= v.y; z /= v.z; w /= v.w; return *this; }

};

template <>
struct LM_ALIGN_16 TVec4<float, SIMD::SSE> : public SIMDTVecBase<float, SIMD::SSE, TVec4, 4>
{

    union
    {
        SIMDT v_;
        struct { VT x, y, z, w; };
    };

    LM_INLINE TVec4()                                       : v_(_mm_set_ps(0.0f, 0.0f, 0.0f, 0.0f)) {}
    LM_INLINE TVec4(ParamT x, ParamT y, ParamT z, ParamT w) : v_(_mm_set_ps(w, z, y, x)) {}
    LM_INLINE TVec4(const VecT& v)                          : v_(v.v_) {}
    LM_INLINE TVec4(SIMDT v)                                : v_(v) {}

    LM_INLINE auto operator[](int i)         -> VT&         { return (&x)[i]; }
    LM_INLINE auto operator[](int i) const   -> RetType     { return (&x)[i]; }
    LM_INLINE auto operator=(const VecT& v)  -> VecT&       { v_ = v.v_; return *this; }
    LM_INLINE auto operator+=(const VecT& v) -> VecT&       { v_ = _mm_add_ps(v_, v.v_); return *this; }
    LM_INLINE auto operator-=(const VecT& v) -> VecT&       { v_ = _mm_sub_ps(v_, v.v_); return *this; }
    LM_INLINE auto operator*=(const VecT& v) -> VecT&       { v_ = _mm_mul_ps(v_, v.v_); return *this; }
    LM_INLINE auto operator/=(const VecT& v) -> VecT&       { v_ = _mm_div_ps(v_, v.v_); return *this; }

};

template <>
struct LM_ALIGN_32 TVec4<double, SIMD::AVX> : public SIMDTVecBase<double, SIMD::AVX, TVec4, 4>
{

    union
    {
        SIMDT v_;
        struct { VT x, y, z, w; };
    };

    LM_INLINE TVec4()                                       : v_(_mm256_set_pd(0, 0, 0, 0)) {}
    LM_INLINE TVec4(ParamT x, ParamT y, ParamT z, ParamT w) : v_(_mm256_set_pd(w, z, y, x)) {}
    LM_INLINE TVec4(const VecT& v)                          : v_(v.v_) {}
    LM_INLINE TVec4(SIMDT v)                                : v_(v) {}

    LM_INLINE auto operator[](int i)         -> VT&         { return (&x)[i]; }
    LM_INLINE auto operator[](int i) const   -> RetType     { return (&x)[i]; }
    LM_INLINE auto operator=(const VecT& v)  -> VecT&       { v_ = v.v_; return *this; }
    LM_INLINE auto operator+=(const VecT& v) -> VecT&       { v_ = _mm256_add_pd(v_, v.v_); return *this; }
    LM_INLINE auto operator-=(const VecT& v) -> VecT&       { v_ = _mm256_sub_pd(v_, v.v_); return *this; }
    LM_INLINE auto operator*=(const VecT& v) -> VecT&       { v_ = _mm256_mul_pd(v_, v.v_); return *this; }
    LM_INLINE auto operator/=(const VecT& v) -> VecT&       { v_ = _mm256_div_pd(v_, v.v_); return *this; }

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

template <typename T, template <typename, SIMD> class VecT>
LM_INLINE auto operator+(const VecTNone<T, VecT>& v1, const VecTNone<T, VecT>& v2) -> VecTNone<T, VecT>
{
    constexpr int N = VecTNone<T, VecT>::NC;
    VecTNone<T, VecT> result;
    for (int i = 0; i < N; i++) result[i] = v1[i] + v2[i];
    return result;
}

template <template <typename, SIMD> class VecT, typename = EnableIfSSEType<VecT>>
LM_INLINE auto operator+(const VecTSSE<VecT>& v1, const VecTSSE<VecT>& v2) -> VecTSSE<VecT>
{
    return VecTSSE<VecT>(_mm_add_ps(v1.v_, v2.v_));
}

template <template <typename, SIMD> class VecT, typename = EnableIfAVXType<VecT>>
LM_INLINE auto operator+(const VecTAVX<VecT>& v1, const VecTAVX<VecT>& v2) -> VecTAVX<VecT>
{
    return VecTAVX<VecT>(_mm256_add_pd(v1.v_, v2.v_));
}

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region operator-

template <typename T, template <typename, SIMD> class VecT>
LM_INLINE auto operator-(const VecTNone<T, VecT>& v1, const VecTNone<T, VecT>& v2) -> VecTNone<T, VecT>
{
    constexpr int N = VecTNone<T, VecT>::NC;
    VecTNone<T, VecT> result;
    for (int i = 0; i < N; i++) result[i] = v1[i] - v2[i];
    return result;
}

template <template <typename, SIMD> class VecT, typename = EnableIfSSEType<VecT>>
LM_INLINE auto operator-(const VecTSSE<VecT>& v1, const VecTSSE<VecT>& v2) -> VecTSSE<VecT>
{
    return VecTSSE<VecT>(_mm_sub_ps(v1.v_, v2.v_));
}

template <template <typename, SIMD> class VecT, typename = EnableIfAVXType<VecT>>
LM_INLINE auto operator-(const VecTAVX<VecT>& v1, const VecTAVX<VecT>& v2) -> VecTAVX<VecT>
{
    return VecTAVX<VecT>(_mm256_sub_pd(v1.v_, v2.v_));
}

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region operator*

template <typename T, template <typename, SIMD> class VecT>
LM_INLINE auto operator*(const VecTNone<T, VecT>& v1, const VecTNone<T, VecT>& v2) -> VecTNone<T, VecT>
{
    constexpr int N = VecTNone<T, VecT>::NC;
    VecTNone<T, VecT> result;
    for (int i = 0; i < N; i++) result[i] = v1[i] * v2[i];
    return result;
}

template <template <typename, SIMD> class VecT, typename = EnableIfSSEType<VecT>>
LM_INLINE auto operator*(const VecTSSE<VecT>& v1, const VecTSSE<VecT>& v2) -> VecTSSE<VecT>
{
    return VecTSSE<VecT>(_mm_mul_ps(v1.v_, v2.v_));
}

template <template <typename, SIMD> class VecT, typename = EnableIfAVXType<VecT>>
LM_INLINE auto operator*(const VecTAVX<VecT>& v1, const VecTAVX<VecT>& v2) -> VecTAVX<VecT>
{
    return VecTAVX<VecT>(_mm256_mul_pd(v1.v_, v2.v_));
}

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region operator/

template <typename T, template <typename, SIMD> class VecT>
LM_INLINE auto operator/(const VecTNone<T, VecT>& v1, const VecTNone<T, VecT>& v2) -> VecTNone<T, VecT>
{
    constexpr int N = VecTNone<T, VecT>::NC;
    VecTNone<T, VecT> result;
    for (int i = 0; i < N; i++) result[i] = v1[i] / v2[i];
    return result;
}

template <template <typename, SIMD> class VecT, typename = EnableIfSSEType<VecT>>
LM_INLINE auto operator/(const VecTSSE<VecT>& v1, const VecTSSE<VecT>& v2) -> VecTSSE<VecT>
{
    return VecTSSE<VecT>(_mm_div_ps(v1.v_, v2.v_));
}

template <template <typename, SIMD> class VecT, typename = EnableIfAVXType<VecT>>
LM_INLINE auto operator/(const VecTAVX<VecT>& v1, const VecTAVX<VecT>& v2) -> VecTAVX<VecT>
{
    return VecTAVX<VecT>(_mm256_div_pd(v1.v_, v2.v_));
}

#pragma endregion

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Matrix types

#pragma region Base matrix type


#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Mat3


#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Mat4



#pragma endregion

// --------------------------------------------------------------------------------

#pragma endregion

LM_NAMESPACE_END
