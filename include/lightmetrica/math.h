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
#include <cmath>
#include <string>
#include <initializer_list>
#include <algorithm>

/*!
    \defgroup math Math library
    \brief A SIMD-optimized math library for renderers

    Math library is the basic constructs for everywhere in the framework.
    We offer the simple yet moderately optimized math library,
    specified for implementing various components in the framework.
    The interface design is heavily inspired by [glm](http://glm.g-truc.net/).
    So those who are familiar with glm would find it easy to use this library.

    ### Precisions
    The library offers configuration to the precision of the floating-point types.
    We can control the underlying floating point types of the framework
    with build parameters for CMake.
    Options:
      - `-D LM_USE_DOUBLE_PRECISION` specifies to use double precision floating-point type.
      - `-D LM_USE_SINGLE_PRECISION` specifies to use single precision floating-point type.

    ### SIMD optimization
    We can use SIMD-optimized functions for various operations.
    We can control the SIMD optimization for basic math types (e.g., `Vec3`, `Mat3`, etc.) with
    the `Opt` template argument. The optimization is currently supported only on `x86` environment
    with two modes: (1) `SSE` for the environment supporting `SSE2`, `SSE3`, and `SSE4.*`, and
    (2) `AVX` for the environment supporting `AVX` and `AVX2`.
    These mode can be controlled by the build options for CMake:
      - `-D LM_USE_SSE` to use `SSE` types (`LM_USE_SINGLE_PRECISION` must be defined)
      - `-D LM_USE_AVX` to use `AVX` types (`LM_USE_DOUBLE_PRECISION` must be defined)

    \{
*/

//! \cond
// TODO. Make configurable from cmake file 
//#define LM_USE_SINGLE_PRECISION
//#define LM_USE_DOUBLE_PRECISION
#define LM_USE_SSE
//#define LM_USE_AVX
//! \endcond

// --------------------------------------------------------------------------------

#pragma region Precision mode

//! \cond
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
//! \endcond

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region SIMD support

//! \cond
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
//! \endcond

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
    \brief SIMD flags.
    \ingroup math_object_types

    Specified SIMD optimization.
    The flag is passed as a template parameter for the basic math types.
      - `None`: No optimization
      - `SSE`: Enables SSE. Requires support of SSE, SSE2, SSE3, SSE4.x
      - `AVX`: Enables AVX. Requires support of AVX, AVX2
      - `Default`: Default optimization flag
          - `SSE` if `LM_USE_SSE` and `LM_USE_SINGLE_PRECISION` are defined
          - `AVX` if `LM_AVX_SSE` and `LM_USE_DOUBLE_PRECISION` are defined
          - `None` otherwise
        
*/
enum class SIMD
{
    None,
    SSE,
    AVX,

    //! \cond
    // Default SIMD type
    #if LM_SINGLE_PRECISION && LM_SSE
    Default = SSE,
    #elif LM_DOUBLE_PRECISION && LM_AVX
    Default = AVX,
    #endif
    //! \endcond
};

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Math object types
//! \defgroup math_object_types Math object types
//! \brief Basic math objects
//! \{

#pragma region Math object type flag
//! \cond

enum class MathObjectType
{
    Vec,
    Mat,
};

//! \endcond
#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Vec4

/*!
    \brief 4D vector.
    
    Generic 4-dimensional vector.

	\tparam T Internal value type.
    \tparam Opt Optimizatoin flag.
*/
template <typename T, SIMD Opt = SIMD::None>
struct TVec4;

//! Default specialization for 4D vector type
template <typename T>
struct TVec4<T, SIMD::None> 
{
    // Math object type
    static constexpr MathObjectType ObjT = MathObjectType::Vec;

    // Value type
    using VT = T;

    // Vector type
    using VecT = TVec4<T, SIMD::None>;

    // Number of components
    static constexpr int NC = 4;

    // Parameter types
    template <typename U>
    using TParam = std::conditional_t<std::is_arithmetic<T>::value, U, const U&>;
    using ParamT = TParam<T>;
    using RetT   = ParamT;

    union
    {
        VT v_[NC];
        struct { VT x, y, z, w; };
    };

    LM_INLINE TVec4()                                       : x(VT(0)), y(VT(0)), z(VT(0)), w(VT(0)) {}
    LM_INLINE TVec4(ParamT x, ParamT y, ParamT z, ParamT w) : x(x), y(y), z(z), w(w) {}
    LM_INLINE TVec4(const VecT& v)                          : x(v.x), y(v.y), z(v.z), w(v.w) {}
    LM_INLINE TVec4(ParamT s)                               : x(s), y(s), z(s), w(s) {}
    LM_INLINE TVec4(std::initializer_list<VT> l)            { x = l.begin()[0]; y = l.begin()[1]; z = l.begin()[2]; w = l.begin()[3]; }

    LM_INLINE auto operator[](int i)         -> VT&         { return (&x)[i]; }
    LM_INLINE auto operator[](int i) const   -> RetT        { return (&x)[i]; }
    LM_INLINE auto operator=(const VecT& v)  -> VecT&       { x = v.x; y = v.y; z = v.z; w = v.w; return *this; }
    LM_INLINE auto operator+=(const VecT& v) -> VecT&       { x += v.x; y += v.y; z += v.z; w += v.w; return *this; }
    LM_INLINE auto operator-=(const VecT& v) -> VecT&       { x -= v.x; y -= v.y; z -= v.z; w -= v.w; return *this; }
    LM_INLINE auto operator*=(const VecT& v) -> VecT&       { x *= v.x; y *= v.y; z *= v.z; w *= v.w; return *this; }
    LM_INLINE auto operator/=(const VecT& v) -> VecT&       { x /= v.x; y /= v.y; z /= v.z; w /= v.w; return *this; }

};

#if LM_SSE
//! Specialization for SIMD optimized 4D vector
template <>
struct LM_ALIGN_16 TVec4<float, SIMD::SSE> 
{
    // Math object type
    static constexpr MathObjectType ObjT = MathObjectType::Vec;

    // Value type
    using VT = float;

    // Vector type
    using VecT = TVec4<float, SIMD::SSE>;

    // Number of components
    static constexpr int NC = 4;

    // Parameter types
    using ParamT = float;
    using RetT   = float;

    using SIMDT = __m128;

    union
    {
        SIMDT v_;
        struct { VT x, y, z, w; };
    };

    LM_INLINE TVec4()                                       : v_(_mm_set_ps(0.0f, 0.0f, 0.0f, 0.0f)) {}
    LM_INLINE TVec4(ParamT x, ParamT y, ParamT z, ParamT w) : v_(_mm_set_ps(w, z, y, x)) {}
    LM_INLINE TVec4(const VecT& v)                          : v_(v.v_) {}
    LM_INLINE TVec4(SIMDT v)                                : v_(v) {}
    LM_INLINE TVec4(ParamT s)                               : v_(_mm_set_ps(s, s, s, s)) {}
    LM_INLINE TVec4(std::initializer_list<VT> l)            { x = l.begin()[0]; y = l.begin()[1]; z = l.begin()[2]; w = l.begin()[3]; }

    LM_INLINE auto operator[](int i)         -> VT&         { return (&x)[i]; }
    LM_INLINE auto operator[](int i) const   -> RetT        { return (&x)[i]; }
    LM_INLINE auto operator=(const VecT& v)  -> VecT&       { v_ = v.v_; return *this; }
    LM_INLINE auto operator+=(const VecT& v) -> VecT&       { v_ = _mm_add_ps(v_, v.v_); return *this; }
    LM_INLINE auto operator-=(const VecT& v) -> VecT&       { v_ = _mm_sub_ps(v_, v.v_); return *this; }
    LM_INLINE auto operator*=(const VecT& v) -> VecT&       { v_ = _mm_mul_ps(v_, v.v_); return *this; }
    LM_INLINE auto operator/=(const VecT& v) -> VecT&       { v_ = _mm_div_ps(v_, v.v_); return *this; }

};
#endif

#if LM_AVX
//! Specialization for AVX optimized 4D vector
template <>
struct LM_ALIGN_32 TVec4<double, SIMD::AVX> 
{
    // Math object type
    static constexpr MathObjectType ObjT = MathObjectType::Vec;

    // Value type
    using VT = double;

    // Vector type
    using VecT = TVec4<double, SIMD::AVX>;

    // Number of components
    static constexpr int NC = 4;

    // Parameter types
    using ParamT = double;
    using RetT   = double;

    using SIMDT = __m256d;

    union
    {
        SIMDT v_;
        struct { VT x, y, z, w; };
    };

    LM_INLINE TVec4()                                       : v_(_mm256_set_pd(0, 0, 0, 0)) {}
    LM_INLINE TVec4(ParamT x, ParamT y, ParamT z, ParamT w) : v_(_mm256_set_pd(w, z, y, x)) {}
    LM_INLINE TVec4(const VecT& v)                          : v_(v.v_) {}
    LM_INLINE TVec4(SIMDT v)                                : v_(v) {}
    LM_INLINE TVec4(ParamT s)                               : v_(_mm256_set_pd(s, s, s, s)) {}
    LM_INLINE TVec4(std::initializer_list<VT> l)            { x = l.begin()[0]; y = l.begin()[1]; z = l.begin()[2]; w = l.begin()[3]; }

    LM_INLINE auto operator[](int i)         -> VT&         { return (&x)[i]; }
    LM_INLINE auto operator[](int i) const   -> RetT        { return (&x)[i]; }
    LM_INLINE auto operator=(const VecT& v)  -> VecT&       { v_ = v.v_; return *this; }
    LM_INLINE auto operator+=(const VecT& v) -> VecT&       { v_ = _mm256_add_pd(v_, v.v_); return *this; }
    LM_INLINE auto operator-=(const VecT& v) -> VecT&       { v_ = _mm256_sub_pd(v_, v.v_); return *this; }
    LM_INLINE auto operator*=(const VecT& v) -> VecT&       { v_ = _mm256_mul_pd(v_, v.v_); return *this; }
    LM_INLINE auto operator/=(const VecT& v) -> VecT&       { v_ = _mm256_div_pd(v_, v.v_); return *this; }

};
#endif

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Vec3

/*!
    \brief 3D vector.
    
    Generic 3-dimensional vector.

	\tparam T Internal value type.
    \tparam Opt Optimizatoin flag.
*/
template <typename T, SIMD Opt = SIMD::None>
struct TVec3;

//! Default specialization for 3D vector type
template <typename T>
struct TVec3<T, SIMD::None> 
{
    // Math object type
    static constexpr MathObjectType ObjT = MathObjectType::Vec;

    // Value type
    using VT = T;

    // Vector type
    using VecT = TVec3<T, SIMD::None>;

    // Number of components
    static constexpr int NC = 3;

    // Parameter types
    template <typename U>
    using TParam = std::conditional_t<std::is_arithmetic<T>::value, U, const U&>;
    using ParamT = TParam<T>;
    using RetT   = ParamT;

    union
    {
        VT v_[NC];
        struct { VT x, y, z; };
    };

    LM_INLINE TVec3()                                   : x(VT(0)), y(VT(0)), z(VT(0)) {}
    LM_INLINE TVec3(ParamT x, ParamT y, ParamT z)       : x(x), y(y), z(z) {}
    LM_INLINE TVec3(const VecT& v)                      : x(v.x), y(v.y), z(v.z) {}
    LM_INLINE TVec3(const TVec4<T, SIMD::None>& v)      : x(v.x), y(v.y), z(v.z) {}
    LM_INLINE TVec3(ParamT s)                           : x(s), y(s), z(s) {}
    LM_INLINE TVec3(std::initializer_list<VT> l)        { x = l.begin()[0]; y = l.begin()[1]; z = l.begin()[2]; }

    LM_INLINE auto operator[](int i)         -> VT&     { return (&x)[i]; }
    LM_INLINE auto operator[](int i) const   -> RetT    { return (&x)[i]; }
    LM_INLINE auto operator=(const VecT& v)  -> VecT&   { x = v.x; y = v.y; z = v.z; return *this; }
    LM_INLINE auto operator+=(const VecT& v) -> VecT&   { x += v.x; y += v.y; z += v.z; return *this; }
    LM_INLINE auto operator-=(const VecT& v) -> VecT&   { x -= v.x; y -= v.y; z -= v.z; return *this; }
    LM_INLINE auto operator*=(const VecT& v) -> VecT&   { x *= v.x; y *= v.y; z *= v.z; return *this; }
    LM_INLINE auto operator/=(const VecT& v) -> VecT&   { x /= v.x; y /= v.y; z /= v.z; return *this; }

};

#if LM_SSE
//! Specialization for SSE optimized 3D vector
template <>
struct LM_ALIGN_16 TVec3<float, SIMD::SSE>
{
    // Math object type
    static constexpr MathObjectType ObjT = MathObjectType::Vec;

    // Value type
    using VT = float;

    // Vector type
    using VecT = TVec3<float, SIMD::SSE>;

    // Number of components
    static constexpr int NC = 3;

    // Parameter types
    using ParamT = float;
    using RetT   = float;

    using SIMDT = __m128;

    union
    {
        SIMDT v_;
        struct { VT x, y, z, _; };
    };

    LM_INLINE TVec3()                                   : v_(_mm_set_ps(1.0f, 0.0f, 0.0f, 0.0f)) {}
    LM_INLINE TVec3(ParamT x, ParamT y, ParamT z)       : v_(_mm_set_ps(1.0f, z, y, x)) {}
    LM_INLINE TVec3(const VecT& v)                      : v_(v.v_) {}
    LM_INLINE TVec3(const TVec4<float, SIMD::SSE>& v)   : v_(v.v_) {}
    LM_INLINE TVec3(SIMDT v)                            : v_(v) {}
    LM_INLINE TVec3(ParamT s)                           : v_(_mm_set_ps(1.0f, s, s, s)) {}
    LM_INLINE TVec3(std::initializer_list<VT> l)        { x = l.begin()[0]; y = l.begin()[1]; z = l.begin()[2]; }

    LM_INLINE auto operator[](int i)         -> VT&     { return (&x)[i]; }
    LM_INLINE auto operator[](int i) const   -> RetT    { return (&x)[i]; }
    LM_INLINE auto operator=(const VecT& v)  -> VecT&   { v_ = v.v_; return *this; }
    LM_INLINE auto operator+=(const VecT& v) -> VecT&   { v_ = _mm_add_ps(v_, v.v_); return *this; }
    LM_INLINE auto operator-=(const VecT& v) -> VecT&   { v_ = _mm_sub_ps(v_, v.v_); return *this; }
    LM_INLINE auto operator*=(const VecT& v) -> VecT&   { v_ = _mm_mul_ps(v_, v.v_); return *this; }
    LM_INLINE auto operator/=(const VecT& v) -> VecT&   { v_ = _mm_div_ps(v_, v.v_); return *this; }

};
#endif

#if LM_AVX
//! Specialization for AVX optimized 3D vector
template <>
struct LM_ALIGN_32 TVec3<double, SIMD::AVX>
{
    // Math object type
    static constexpr MathObjectType ObjT = MathObjectType::Vec;

    // Value type
    using VT = double;

    // Vector type
    using VecT = TVec3<double, SIMD::AVX>;

    // Number of components
    static constexpr int NC = 3;

    // Parameter types
    using ParamT = double;
    using RetT   = double;

    using SIMDT = __m256d;

    union
    {
        SIMDT v_;
        struct { VT x, y, z, _; };
    };

    LM_INLINE TVec3()                                   : v_(_mm256_set_pd(1, 0, 0, 0)) {}
    LM_INLINE TVec3(ParamT x, ParamT y, ParamT z)       : v_(_mm256_set_pd(1, z, y, x)) {}
    LM_INLINE TVec3(const VecT& v)                      : v_(v.v_) {}
    LM_INLINE TVec3(const TVec4<double, SIMD::AVX>& v)  : v_(v.v_) {}
    LM_INLINE TVec3(SIMDT v)                            : v_(v) {}
    LM_INLINE TVec3(ParamT s)                           : v_(_mm256_set_pd(1, s, s, s)) {}
    LM_INLINE TVec3(std::initializer_list<VT> l)        { x = l.begin()[0]; y = l.begin()[1]; z = l.begin()[2]; }

    LM_INLINE auto operator[](int i)         -> VT&     { return (&x)[i]; }
    LM_INLINE auto operator[](int i) const   -> RetT    { return (&x)[i]; }
    LM_INLINE auto operator=(const VecT& v)  -> VecT&   { v_ = v.v_; return *this; }
    LM_INLINE auto operator+=(const VecT& v) -> VecT&   { v_ = _mm256_add_pd(v_, v.v_); return *this; }
    LM_INLINE auto operator-=(const VecT& v) -> VecT&   { v_ = _mm256_sub_pd(v_, v.v_); return *this; }
    LM_INLINE auto operator*=(const VecT& v) -> VecT&   { v_ = _mm256_mul_pd(v_, v.v_); return *this; }
    LM_INLINE auto operator/=(const VecT& v) -> VecT&   { v_ = _mm256_div_pd(v_, v.v_); return *this; }

};
#endif

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Vec2

/*!
    \brief 2D vector.
    
    Generic 2-dimensional vector.

	\tparam T Internal value type.
    \tparam Opt Optimizatoin flag.
*/
template <typename T, SIMD Opt = SIMD::None>
struct TVec2;

//! Default specialization for 2D vector type
template <typename T>
struct TVec2<T, SIMD::None>
{
    // Math object type
    static constexpr MathObjectType ObjT = MathObjectType::Vec;

    // Value type
    using VT = T;

    // Vector type
    using VecT = TVec3<T, SIMD::None>;

    // Number of components
    static constexpr int NC = 2;

    // Parameter types
    template <typename U>
    using TParam = std::conditional_t<std::is_arithmetic<T>::value, U, const U&>;
    using ParamT = TParam<T>;
    using RetT   = ParamT;

    union
    {
        VT v_[NC];
        struct { VT x, y; };
    };

    LM_INLINE TVec2() : x(VT(0)), y(VT(0)) {}
    LM_INLINE TVec2(ParamT x, ParamT y) : x(x), y(y) {}
    LM_INLINE TVec2(const VecT& v) : x(v.x), y(v.y) {}
    LM_INLINE TVec2(ParamT s) : x(s), y(s) {}
    LM_INLINE TVec2(std::initializer_list<VT> l) { x = l.begin()[0]; y = l.begin()[1]; }

    LM_INLINE auto operator[](int i)         -> VT& { return (&x)[i]; }
    LM_INLINE auto operator[](int i) const   -> RetT { return (&x)[i]; }
    LM_INLINE auto operator=(const VecT& v)  -> VecT& { x = v.x; y = v.y; return *this; }
    LM_INLINE auto operator+=(const VecT& v) -> VecT& { x += v.x; y += v.y; return *this; }
    LM_INLINE auto operator-=(const VecT& v) -> VecT& { x -= v.x; y -= v.y; return *this; }
    LM_INLINE auto operator*=(const VecT& v) -> VecT& { x *= v.x; y *= v.y; return *this; }
    LM_INLINE auto operator/=(const VecT& v) -> VecT& { x /= v.x; y /= v.y; return *this; }

};

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Mat4

/*!
	\brief 4x4 matrix.

	Generic column major 4x4 matrix. 
	A matrix
		v00 v01 v02 v03
		v10 v11 v12 v13
		v20 v21 v22 v23
		v30 v31 v32 v33
	is stored sequentially as v00, v10, ..., v33

    \tparam T Internal value type.
    \tparam Opt Optimizatoin flag.
*/
template <typename T, SIMD Opt = SIMD::None>
struct TMat4
{
    // Math object type
    static constexpr MathObjectType ObjT = MathObjectType::Mat;

    // Value type
    using VT = T;

    // Matrix type
    using MatT = TMat4<T, Opt>;

    // Column vector type
    template <typename T_, SIMD Opt_>
    using TVec = TVec4<T_, Opt_>;
    using VecT = TVec4<T, Opt>;

    // Number of components
    static constexpr int NC = 4;

    // Parameter types
    using ParamT = std::conditional_t<std::is_fundamental<T>::value, T, const T&>;
    using RetT = ParamT;

    VecT v_[NC];

    LM_INLINE TMat4() {}
    LM_INLINE TMat4(const MatT& m) : v_{m.v_[0], m.v_[1], m.v_[2], m.v_[3]} {}
    LM_INLINE TMat4(const VecT& v0, const VecT& v1, const VecT& v2, const VecT& v3) : v_{v0, v1, v2, v3} {}
    LM_INLINE TMat4(const T& s) : v_{VecT(s), VecT(s), VecT(s), VecT(s)} {}
    LM_INLINE TMat4(
        ParamT v00, ParamT v10, ParamT v20, ParamT v30,
        ParamT v01, ParamT v11, ParamT v21, ParamT v31,
        ParamT v02, ParamT v12, ParamT v22, ParamT v32,
        ParamT v03, ParamT v13, ParamT v23, ParamT v33)
        : v_{VecT(v00, v10, v20, v30),
             VecT(v01, v11, v21, v31),
             VecT(v02, v12, v22, v32),
             VecT(v03, v13, v23, v33)}
    {}
    LM_INLINE TMat4(std::initializer_list<VT> l)
        : v_{VecT(l.begin()[ 0], l.begin()[ 1], l.begin()[ 2], l.begin()[ 3]),
             VecT(l.begin()[ 4], l.begin()[ 5], l.begin()[ 6], l.begin()[ 7]),
             VecT(l.begin()[ 8], l.begin()[ 9], l.begin()[10], l.begin()[11]),
             VecT(l.begin()[12], l.begin()[13], l.begin()[14], l.begin()[15])}
    {}

    static auto Identity() -> MatT { return MatT{ 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 }; }
    
	LM_INLINE auto operator[](int i) -> VecT& { return v_[i]; }
    LM_INLINE auto operator[](int i) const -> const VecT& { return v_[i]; }
    LM_INLINE auto operator*=(const MatT& m) -> MatT& { *this = *this * m; return *this; }

};

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Mat3

/*!
	\brief 3x3 matrix.

    Generic column major 3x3 matrix.
    A matrix
        v00 v01 v02
        v10 v11 v12
        v20 v21 v22
    is stored sequentially as v00, v10, ..., v22.

    \tparam T Internal value type.
    \tparam Opt Optimizatoin flag.
*/
template <typename T, SIMD Opt = SIMD::None>
struct TMat3
{
    // Math object type
    static constexpr MathObjectType ObjT = MathObjectType::Mat;

    // Value type
    using VT = T;

    // Matrix type
    using MatT = TMat3<T, Opt>;

    // Column vector type
    template <typename T_, SIMD Opt_>
    using TVec = TVec3<T_, Opt_>;
    using VecT = TVec3<T, Opt>;

    // Number of components
    static constexpr int NC = 3;

    // Parameter types
    using ParamT = std::conditional_t<std::is_fundamental<T>::value, T, const T&>;
    using RetT = ParamT;

    VecT v_[NC];

    LM_INLINE TMat3() {}
    LM_INLINE TMat3(const MatT& m) : v_{m.v_[0], m.v_[1], m.v_[2]} {}
    LM_INLINE TMat3(const TMat4<T, Opt>& m) : v_{VecT(m.v_[0]), VecT(m.v_[1]), VecT(m.v_[2])} {}
    LM_INLINE TMat3(const VecT& v0, const VecT& v1, const VecT& v2) : v_{v0, v1, v2} {}
    LM_INLINE TMat3(const T& s) : v_{VecT(s), VecT(s), VecT(s)} {}
    LM_INLINE TMat3(
        ParamT v00, ParamT v10, ParamT v20,
        ParamT v01, ParamT v11, ParamT v21,
        ParamT v02, ParamT v12, ParamT v22)
        : v_{VecT(v00, v10, v20),
             VecT(v01, v11, v21),
             VecT(v02, v12, v22)}
    {}
    LM_INLINE TMat3(std::initializer_list<VT> l)
        : v_{VecT(l.begin()[0], l.begin()[1], l.begin()[2]),
             VecT(l.begin()[3], l.begin()[4], l.begin()[5]),
             VecT(l.begin()[6], l.begin()[7], l.begin()[8])}
    {}

    static auto Identity() -> MatT { return MatT{ 1,0,0, 0,1,0, 0,1,0 }; }

	LM_INLINE auto operator[](int i) -> VecT& { return v_[i]; }
	LM_INLINE auto operator[](int i) const -> const VecT& { return v_[i]; }
    LM_INLINE auto operator*=(const MatT& m) -> MatT& { *this = *this * m; return *this; }

};

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Some aliases

template <typename T, SIMD Opt, template <typename, SIMD> class MathObject>
using EnableIfVecType = std::enable_if_t<MathObject<T, Opt>::ObjT == MathObjectType::Vec>;

template <typename T, SIMD Opt, template <typename, SIMD> class MathObject>
using EnableIfMatType = std::enable_if_t<MathObject<T, Opt>::ObjT == MathObjectType::Mat>;

template <typename T, SIMD Opt>
using EnableIfSSEType = std::enable_if_t<std::is_same<T, float>::value && Opt == SIMD::SSE>;

template <typename T, SIMD Opt>
using EnableIfAVXType = std::enable_if_t<std::is_same<T, double>::value && Opt == SIMD::AVX>;

#pragma endregion

//! \}
#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Math operations
/*!
    \addtogroup math_object_types
    \{
*/

#pragma region operator+

template <typename T, template <typename, SIMD> class VecT>
LM_INLINE auto operator+(const VecT<T, SIMD::None>& v1, const VecT<T, SIMD::None>& v2) -> VecT<T, SIMD::None>
{
    constexpr int N = VecT<T, SIMD::None>::NC;
    VecT<T, SIMD::None> result;
    for (int i = 0; i < N; i++) result[i] = v1[i] + v2[i];
    return result;
}

template <template <typename, SIMD> class VecT>
LM_INLINE auto operator+(const VecT<float, SIMD::SSE>& v1, const VecT<float, SIMD::SSE>& v2) -> VecT<float, SIMD::SSE>
{
    return VecT<float, SIMD::SSE>(_mm_add_ps(v1.v_, v2.v_));
}

template <template <typename, SIMD> class VecT>
LM_INLINE auto operator+(const VecT<double, SIMD::AVX>& v1, const VecT<double, SIMD::AVX>& v2) -> VecT<double, SIMD::AVX>
{
    return VecT<double, SIMD::AVX>(_mm256_add_pd(v1.v_, v2.v_));
}

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region operator-

template <typename T, template <typename, SIMD> class VecT>
LM_INLINE auto operator-(const VecT<T, SIMD::None>& v1, const VecT<T, SIMD::None>& v2) -> VecT<T, SIMD::None>
{
    constexpr int N = VecT<T, SIMD::None>::NC;
    VecT<T, SIMD::None> result;
    for (int i = 0; i < N; i++) result[i] = v1[i] - v2[i];
    return result;
}

template <template <typename, SIMD> class VecT>
LM_INLINE auto operator-(const VecT<float, SIMD::SSE>& v1, const VecT<float, SIMD::SSE>& v2) -> VecT<float, SIMD::SSE>
{
    return VecT<float, SIMD::SSE>(_mm_sub_ps(v1.v_, v2.v_));
}

template <template <typename, SIMD> class VecT>
LM_INLINE auto operator-(const VecT<double, SIMD::AVX>& v1, const VecT<double, SIMD::AVX>& v2) -> VecT<double, SIMD::AVX>
{
    return VecT<double, SIMD::AVX>(_mm256_sub_pd(v1.v_, v2.v_));
}

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region operator*

#pragma region Vec * Vec

template <
    typename T,
    SIMD Opt,
    template <typename, SIMD> class VecT,
    typename = EnableIfVecType<T, Opt, VecT>
>
LM_INLINE auto operator*(const VecT<T, Opt>& v1, const VecT<T, Opt>& v2) -> VecT<T, Opt>
{
    constexpr int N = VecT<T, Opt>::NC;
    VecT<T, Opt> result;
    for (int i = 0; i < N; i++) result[i] = v1[i] * v2[i];
    return result;
}

#if LM_SSE
template <>
LM_INLINE auto operator*<float, SIMD::SSE, TVec3>(const TVec3<float, SIMD::SSE>& v1, const TVec3<float, SIMD::SSE>& v2) -> TVec3<float, SIMD::SSE>
{
    return TVec3<float, SIMD::SSE>(_mm_mul_ps(v1.v_, v2.v_));
}

template <>
LM_INLINE auto operator*<float, SIMD::SSE, TVec4>(const TVec4<float, SIMD::SSE>& v1, const TVec4<float, SIMD::SSE>& v2) -> TVec4<float, SIMD::SSE>
{
    return TVec4<float, SIMD::SSE>(_mm_mul_ps(v1.v_, v2.v_));
}
#endif

#if LM_AVX
template <>
LM_INLINE auto operator*<double, SIMD::AVX, TVec3>(const TVec3<double, SIMD::AVX>& v1, const TVec3<double, SIMD::AVX>& v2) -> TVec3<double, SIMD::AVX>
{
    return TVec3<double, SIMD::AVX>(_mm256_mul_pd(v1.v_, v2.v_));
}

template <>
LM_INLINE auto operator*<double, SIMD::AVX, TVec4>(const TVec4<double, SIMD::AVX>& v1, const TVec4<double, SIMD::AVX>& v2) -> TVec4<double, SIMD::AVX>
{
    return TVec4<double, SIMD::AVX>(_mm256_mul_pd(v1.v_, v2.v_));
}
#endif

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Vec * Scalar

template <
    typename T,
    SIMD Opt,
    template <typename, SIMD> class VecT,
    typename = EnableIfVecType<T, Opt, VecT>
>
LM_INLINE auto operator*(const VecT<T, Opt>& v, const T& s) -> VecT<T, Opt>
{
    constexpr int N = VecT<T, Opt>::NC;
    VecT<T, Opt> result;
    for (int i = 0; i < N; i++) result[i] = v[i] * s;
    return result;
}

#if LM_SSE
template <>
LM_INLINE auto operator*<float, SIMD::SSE, TVec3>(const TVec3<float, SIMD::SSE>& v, const float& s) -> TVec3<float, SIMD::SSE>
{
    return TVec3<float, SIMD::SSE>(_mm_mul_ps(v.v_, _mm_set1_ps(s)));
}

template <>
LM_INLINE auto operator*<float, SIMD::SSE, TVec4>(const TVec4<float, SIMD::SSE>& v, const float& s) -> TVec4<float, SIMD::SSE>
{
    return TVec4<float, SIMD::SSE>(_mm_mul_ps(v.v_, _mm_set1_ps(s)));
}
#endif

#if LM_AVX
template <>
LM_INLINE auto operator*<double, SIMD::AVX, TVec3>(const TVec3<double, SIMD::AVX>& v, const double& s) -> TVec3<double, SIMD::AVX>
{
    return TVec3<double, SIMD::AVX>(_mm256_mul_pd(v.v_, _mm256_set1_pd(s)));
}

template <>
LM_INLINE auto operator*<double, SIMD::AVX, TVec4>(const TVec4<double, SIMD::AVX>& v, const double& s) -> TVec4<double, SIMD::AVX>
{
    return TVec4<double, SIMD::AVX>(_mm256_mul_pd(v.v_, _mm256_set1_pd(s)));
}
#endif

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Scalar * Vec

template <typename T, SIMD Opt, template <typename, SIMD> class VecT, typename = EnableIfVecType<T, Opt, VecT>>
LM_INLINE auto operator*(const T& s, const VecT<T, Opt>& v) -> VecT<T, Opt>
{
    return v * s;
}

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Mat * Mat

template <typename T, SIMD Opt>
LM_INLINE auto operator*(const TMat3<T, Opt>& m1, const TMat3<T, Opt>& m2) -> TMat3<T, Opt>
{
    return TMat3<T, Opt>(m1 * m2[0], m1 * m2[1], m1 * m2[2]);
}

template <typename T, SIMD Opt>
LM_INLINE auto operator*(const TMat4<T, Opt>& m1, const TMat4<T, Opt>& m2) -> TMat4<T, Opt>
{
    return TMat4<T, Opt>(m1 * m2[0], m1 * m2[1], m1 * m2[2], m1 * m2[3]);
}

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Mat * Vec

template <typename T, SIMD Opt>
LM_INLINE auto operator*(const TMat3<T, Opt>& m, const TVec3<T, Opt>& v) -> TVec3<T, Opt>
{
    return TVec3<T, Opt>(
        m[0][0] * v.x + m[1][0] * v.y + m[2][0] * v.z,
        m[0][1] * v.x + m[1][1] * v.y + m[2][1] * v.z,
        m[0][2] * v.x + m[1][2] * v.y + m[2][2] * v.z);
}

#if LM_SSE
template <>
LM_INLINE auto operator*<float, SIMD::SSE>(const TMat3<float, SIMD::SSE>& m, const TVec3<float, SIMD::SSE>& v) -> TVec3<float, SIMD::SSE>
{
	return TVec3<float, SIMD::SSE>(
        _mm_add_ps(
            _mm_add_ps(
                _mm_mul_ps(m[0].v_, _mm_shuffle_ps(v.v_, v.v_, _MM_SHUFFLE(0, 0, 0, 0))),
                _mm_mul_ps(m[1].v_, _mm_shuffle_ps(v.v_, v.v_, _MM_SHUFFLE(1, 1, 1, 1)))),
                _mm_mul_ps(m[2].v_, _mm_shuffle_ps(v.v_, v.v_, _MM_SHUFFLE(2, 2, 2, 2)))));
}
#endif

#if LM_AVX
template <>
LM_INLINE auto operator*<double, SIMD::AVX>(const TMat3<double, SIMD::AVX>& m, const TVec3<double, SIMD::AVX>& v) -> TVec3<double, SIMD::AVX>
{
    return TVec3<double, SIMD::AVX>(
        _mm256_add_pd(
            _mm256_add_pd(
                _mm256_mul_pd(m[0].v_, _mm256_broadcast_sd(&(v.x))),
                _mm256_mul_pd(m[1].v_, _mm256_broadcast_sd(&(v.x) + 1))),
                _mm256_mul_pd(m[2].v_, _mm256_broadcast_sd(&(v.x) + 2))));
}
#endif

template <typename T, SIMD Opt>
LM_INLINE auto operator*(const TMat4<T, Opt>& m, const TVec4<T, Opt>& v) -> TVec4<T, Opt>
{
    return TVec4<T, Opt>(
        m[0][0] * v.x + m[1][0] * v.y + m[2][0] * v.z + m[3][0] * v.w,
        m[0][1] * v.x + m[1][1] * v.y + m[2][1] * v.z + m[3][1] * v.w,
        m[0][2] * v.x + m[1][2] * v.y + m[2][2] * v.z + m[3][2] * v.w,
        m[0][3] * v.x + m[1][3] * v.y + m[2][3] * v.z + m[3][3] * v.w);
}

#if LM_SSE
template <>
LM_INLINE auto operator*<float, SIMD::SSE>(const TMat4<float, SIMD::SSE>& m, const TVec4<float, SIMD::SSE>& v) -> TVec4<float, SIMD::SSE>
{
    return TVec4<float, SIMD::SSE>(
        _mm_add_ps(
            _mm_add_ps(
                _mm_mul_ps(m[0].v_, _mm_shuffle_ps(v.v_, v.v_, _MM_SHUFFLE(0, 0, 0, 0))),
                _mm_mul_ps(m[1].v_, _mm_shuffle_ps(v.v_, v.v_, _MM_SHUFFLE(1, 1, 1, 1)))),
            _mm_add_ps(
                _mm_mul_ps(m[2].v_, _mm_shuffle_ps(v.v_, v.v_, _MM_SHUFFLE(2, 2, 2, 2))),
                _mm_mul_ps(m[3].v_, _mm_shuffle_ps(v.v_, v.v_, _MM_SHUFFLE(3, 3, 3, 3))))));
}
#endif

#if LM_AVX
template <>
LM_INLINE auto operator*<double, SIMD::AVX>(const TMat4<double, SIMD::AVX>& m, const TVec4<double, SIMD::AVX>& v) -> TVec4<double, SIMD::AVX>
{
    return TVec4<double, SIMD::AVX>(
        _mm256_add_pd(
            _mm256_add_pd(
                _mm256_mul_pd(m[0].v_, _mm256_broadcast_sd(&(v.x))),
                _mm256_mul_pd(m[1].v_, _mm256_broadcast_sd(&(v.x) + 1))),
            _mm256_add_pd(
                _mm256_mul_pd(m[2].v_, _mm256_broadcast_sd(&(v.x) + 2)),
                _mm256_mul_pd(m[3].v_, _mm256_broadcast_sd(&(v.x) + 3)))));
}
#endif

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Mat * Scalar

template <typename T, SIMD Opt>
LM_INLINE auto operator*(const TMat3<T, Opt>& m, const T& s) -> TMat3<T, Opt>
{
    return TMat3<T, Opt>(m[0] * s, m[1] * s, m[2] * s);
}


template <typename T, SIMD Opt>
LM_INLINE auto operator*(const TMat4<T, Opt>& m, const T& s) -> TMat4<T, Opt>
{
    return TMat4<T, Opt>(m[0] * s, m[1] * s, m[2] * s, m[3] * s);
}

#pragma endregion

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region operator/

#pragma region Vec / Vec

template <
    typename T,
    SIMD Opt,
    template <typename, SIMD> class VecT>
LM_INLINE auto operator/(const VecT<T, Opt>& v1, const VecT<T, Opt>& v2) -> VecT<T, Opt>
{
    constexpr int N = VecT<T, Opt>::NC;
    VecT<T, Opt> result;
    for (int i = 0; i < N; i++) result[i] = v1[i] / v2[i];
    return result;
}

#if LM_SSE
template <>
LM_INLINE auto operator/<float, SIMD::SSE, TVec3>(const TVec3<float, SIMD::SSE>& v1, const TVec3<float, SIMD::SSE>& v2) -> TVec3<float, SIMD::SSE>
{
    return TVec3<float, SIMD::SSE>(_mm_div_ps(v1.v_, v2.v_));
}

template <>
LM_INLINE auto operator/<float, SIMD::SSE, TVec4>(const TVec4<float, SIMD::SSE>& v1, const TVec4<float, SIMD::SSE>& v2) -> TVec4<float, SIMD::SSE>
{
    return TVec4<float, SIMD::SSE>(_mm_div_ps(v1.v_, v2.v_));
}
#endif

#if LM_AVX
template <>
LM_INLINE auto operator/<double, SIMD::AVX>(const TVec3<double, SIMD::AVX>& v1, const TVec3<double, SIMD::AVX>& v2) -> TVec3<double, SIMD::AVX>
{
    return TVec3<double, SIMD::AVX>(_mm256_div_pd(v1.v_, v2.v_));
}

template <>
LM_INLINE auto operator/<double, SIMD::AVX>(const TVec4<double, SIMD::AVX>& v1, const TVec4<double, SIMD::AVX>& v2) -> TVec4<double, SIMD::AVX>
{
    return TVec4<double, SIMD::AVX>(_mm256_div_pd(v1.v_, v2.v_));
}
#endif

#pragma endregion

#pragma region Vec / Scalar

template <
    typename T,
    SIMD Opt,
    template <typename, SIMD> class VecT,
    typename = EnableIfVecType<T, Opt, VecT>
>
LM_INLINE auto operator/(const VecT<T, Opt>& v, const T& s) -> VecT<T, Opt>
{
    constexpr int N = VecT<T, Opt>::NC;
    VecT<T, Opt> result;
    for (int i = 0; i < N; i++) result[i] = v[i] / s;
    return result;
}

#if LM_SSE
template <>
LM_INLINE auto operator/<float, SIMD::SSE, TVec3>(const TVec3<float, SIMD::SSE>& v, const float& s) -> TVec3<float, SIMD::SSE>
{
    return TVec3<float, SIMD::SSE>(_mm_div_ps(v.v_, _mm_set1_ps(s)));
}

template <>
LM_INLINE auto operator/<float, SIMD::SSE, TVec4>(const TVec4<float, SIMD::SSE>& v, const float& s) -> TVec4<float, SIMD::SSE>
{
    return TVec4<float, SIMD::SSE>(_mm_div_ps(v.v_, _mm_set1_ps(s)));
}
#endif

#if LM_AVX
template <>
LM_INLINE auto operator/<double, SIMD::AVX, TVec3>(const TVec3<double, SIMD::AVX>& v, const double& s) -> TVec3<double, SIMD::AVX>
{
    return TVec3<double, SIMD::AVX>(_mm256_div_pd(v.v_, _mm256_set1_pd(s)));
}

template <>
LM_INLINE auto operator/<double, SIMD::AVX, TVec4>(const TVec4<double, SIMD::AVX>& v, const double& s) -> TVec4<double, SIMD::AVX>
{
    return TVec4<double, SIMD::AVX>(_mm256_div_pd(v.v_, _mm256_set1_pd(s)));
}
#endif

#pragma endregion

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Comparators

template <
    typename T,
    SIMD Opt,
    template <typename, SIMD> class VecT,
    typename = EnableIfVecType<T, Opt, VecT>
>
LM_INLINE auto operator==(const VecT<T, Opt>& v1, const VecT<T, Opt>& v2) -> bool
{
    constexpr int N = VecT<T, Opt>::NC;
    for (int i = 0; i < N; i++) if (v1[i] != v2[i]) return false;
    return true;
}

template <
    typename T,
    SIMD Opt,
    template <typename, SIMD> class VecT,
    typename = EnableIfVecType<T, Opt, VecT>
>
LM_INLINE auto operator==(const VecT<T, Opt>& v, const T& s) -> bool
{
    return v == VecT<T, Opt>(s);
}

template <
    typename T,
    SIMD Opt,
    template <typename, SIMD> class VecT,
    typename = EnableIfVecType<T, Opt, VecT>
>
LM_INLINE auto operator==(const VecT<T, Opt>& v, int s) -> bool
{
    return v == VecT<T, Opt>(T(s));
}

template <
    typename T,
    SIMD Opt,
    template <typename, SIMD> class VecT,
    typename = EnableIfVecType<T, Opt, VecT>
>
LM_INLINE auto operator!=(const VecT<T, Opt>& v1, const VecT<T, Opt>& v2) -> bool
{
    constexpr int N = VecT<T, Opt>::NC;
    for (int i = 0; i < N; i++) if (v1[i] != v2[i]) return true;
    return false;
}

template <
    typename T,
    SIMD Opt,
    template <typename, SIMD> class VecT,
    typename = EnableIfVecType<T, Opt, VecT>
>
LM_INLINE auto operator!=(const VecT<T, Opt>& v, const T& s) -> bool
{
    return v == VecT<T, Opt>(s);
}

template <
    typename T,
    SIMD Opt,
    template <typename, SIMD> class VecT,
    typename = EnableIfVecType<T, Opt, VecT>
>
LM_INLINE auto operator<(const VecT<T, Opt>& v1, const VecT<T, Opt>& v2) -> bool
{
    constexpr int N = VecT<T, Opt>::NC;
    for (int i = 0; i < N; i++) if (v1[i] >= v2[i]) return false;
    return true;
}

template <
    typename T,
    SIMD Opt,
    template <typename, SIMD> class VecT,
    typename = EnableIfVecType<T, Opt, VecT>
>
LM_INLINE auto operator<(const VecT<T, Opt>& v, const T& s) -> bool
{
    return v < VecT<T, Opt>(s);
}

template <
    typename T,
    SIMD Opt,
    template <typename, SIMD> class VecT,
    typename = EnableIfVecType<T, Opt, VecT>
>
LM_INLINE auto operator>(const VecT<T, Opt>& v1, const VecT<T, Opt>& v2) -> bool
{
    constexpr int N = VecT<T, Opt>::NC;
    for (int i = 0; i < N; i++) if (v1[i] <= v2[i]) return false;
    return true;
}

template <
    typename T,
    SIMD Opt,
    template <typename, SIMD> class VecT,
    typename = EnableIfVecType<T, Opt, VecT>
>
LM_INLINE auto operator>(const VecT<T, Opt>& v, const T& s) -> bool
{
    return v > VecT<T, Opt>(s);
}

template <
    typename T,
    SIMD Opt,
    template <typename, SIMD> class VecT,
    typename = EnableIfVecType<T, Opt, VecT>
>
LM_INLINE auto operator<=(const VecT<T, Opt>& v1, const VecT<T, Opt>& v2) -> bool
{
    return v1 < v2 || v1 == v2;
}

template <
    typename T,
    SIMD Opt,
    template <typename, SIMD> class VecT,
    typename = EnableIfVecType<T, Opt, VecT>
>
LM_INLINE auto operator<=(const VecT<T, Opt>& v, const T& s) -> bool
{
    return v <= VecT<T, Opt>(s);
}

template <
    typename T,
    SIMD Opt,
    template <typename, SIMD> class VecT,
    typename = EnableIfVecType<T, Opt, VecT>
>
LM_INLINE auto operator>=(const VecT<T, Opt>& v1, const VecT<T, Opt>& v2) -> bool
{
    return v1 > v2 || v1 == v2;
}

template <
    typename T,
    SIMD Opt,
    template <typename, SIMD> class VecT,
    typename = EnableIfVecType<T, Opt, VecT>
>
LM_INLINE auto operator>=(const VecT<T, Opt>& v, const T& s) -> bool
{
    return v >= VecT<T, Opt>(s);
}

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region operator- (unary)

template <typename T, template <typename, SIMD> class VecT>
LM_INLINE auto operator-(const VecT<T, SIMD::None>& v) -> VecT<T, SIMD::None>
{
    constexpr int N = VecT<T, SIMD::None>::NC;
    VecT<T, SIMD::None> result;
    for (int i = 0; i < N; i++) result[i] = -v[i];
    return result;
}

#if LM_SSE
template <template <typename, SIMD> class VecT>
LM_INLINE auto operator-(const VecT<float, SIMD::SSE>& v) -> VecT<float, SIMD::SSE>
{
    return VecT<float, SIMD::SSE>(_mm_sub_ps(_mm_setzero_ps(), v.v_));
}
#endif

#if LM_AVX
template <template <typename, SIMD> class VecT>
LM_INLINE auto operator-(const VecT<double, SIMD::AVX>& v) -> VecT<double, SIMD::AVX>
{
    return VecT<double, SIMD::AVX>(_mm256_sub_pd(_mm256_setzero_pd(), v.v_));
}
#endif

#pragma endregion

/*!
    \}
*/
#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Default types
//! \defgroup default_types Default types
//! \brief Default math object types.
//! \{

using Vec2 = TVec2<Float, SIMD::None>;
using Vec3 = TVec3<Float, SIMD::Default>;
using Vec4 = TVec4<Float, SIMD::Default>;
using Mat3 = TMat3<Float, SIMD::Default>;
using Mat4 = TMat4<Float, SIMD::Default>;

//! \}
#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Math utility
/*!
    \defgroup math_utils Math utility
    \brief Math functions for various basic types.

    Defines various math functions for vector and matrix types.
    \{
*/

//! Utility functions for vector and matrix types
namespace Math
{
    #pragma region Constants

    template <typename T = Float> constexpr auto Pi()              -> T     { return T(3.141592653589793238462643383279502884e+00); }
    template <typename T = Float> constexpr auto InvPi()           -> T     { return T(1.0 / 3.141592653589793238462643383279502884e+00); }
    template <typename T = Float> constexpr auto Inf()             -> T     { return std::numeric_limits<T>::max(); }
    template <typename T = Float> constexpr auto EpsLarge()        -> T     { return T(1e-5); }
    template <>                   constexpr auto EpsLarge<float>() -> float { return 1e-3f; }
    template <typename T = Float> constexpr auto Eps()             -> T     { return T(1e-7); }
    template <>                   constexpr auto Eps<float>()      -> float { return 1e-4f; }
    template <typename T = Float> constexpr auto EpsIsect()        -> T     { return T(1e-4); }

    #pragma endregion

    // --------------------------------------------------------------------------------

    #pragma region Basic functions

    template <typename T> constexpr auto Radians(const T& v) -> T { return v * Pi<T>() / T(180); }
    template <typename T> constexpr auto Degrees(const T& v) -> T { return v * T(180) / Pi<T>(); }
    template <typename T> LM_INLINE auto Cos(const T& v)     -> T { return std::cos(v); }
    template <typename T> LM_INLINE auto Sin(const T& v)     -> T { return std::sin(v); }
    template <typename T> LM_INLINE auto Tan(const T& v)     -> T { return std::tan(v); }
    template <typename T> LM_INLINE auto Acos(const T& v)    -> T { return std::acos(v); }
    template <typename T> LM_INLINE auto Abs(const T& v)     -> T { return std::abs(v); }
    template <typename T> LM_INLINE auto Sqrt(const T& v)    -> T { return std::sqrt(v); }
    template <typename T> LM_INLINE auto Fract(const T& v)   -> T { return v - std::floor(v); }
    template <typename T> LM_INLINE auto Pow(const T& base, const T& exp) -> T { return std::pow(base, exp); }
    template <typename T> LM_INLINE auto Min(const T& v1, const T& v2)    -> T { return std::min(v1, v2); }
    template <typename T> LM_INLINE auto Max(const T& v1, const T& v2)    -> T { return std::max(v1, v2); }
    template <typename T> LM_INLINE auto Clamp(const T& v, const T& min, const T& max) -> T { return std::min(std::max(v, min), max); }

    template <typename T, SIMD Opt, template <typename, SIMD> class VecT>
    LM_INLINE auto Min(const VecT<T, Opt>& v1, const VecT<T, Opt>& v2) -> VecT<T, Opt>
    {
        VecT<T, Opt> result;
        for (int i = 0; i < VecT<T, Opt>::NC; i++) result[i] = Min<T>(v1[i], v2[i]);
        return result;
    }

    template <typename T, SIMD Opt, template <typename, SIMD> class VecT>
    LM_INLINE auto Max(const VecT<T, Opt>& v1, const VecT<T, Opt>& v2) -> VecT<T, Opt>
    {
        VecT<T, Opt> result;
        for (int i = 0; i < VecT<T, Opt>::NC; i++) result[i] = Max<T>(v1[i], v2[i]);
        return result;
    }

    #pragma endregion

    // --------------------------------------------------------------------------------

    #pragma region Vector functions

    #pragma region Dot

    template <typename T, SIMD Opt>
    LM_INLINE auto Dot(const TVec3<T, Opt>& v1, const TVec3<T, Opt>& v2) -> T
    {
        return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
    }

    #if LM_SSE
    template <>
    LM_INLINE auto Dot<float, SIMD::SSE>(const TVec3<float, SIMD::SSE>& v1, const TVec3<float, SIMD::SSE>& v2) -> float
    {
        return _mm_cvtss_f32(_mm_dp_ps(v1.v_, v2.v_, 0x71));
    }
    #endif

    #if LM_AVX
    template <>
    LM_INLINE auto Dot<double, SIMD::AVX>(const TVec3<double, SIMD::AVX>& v1, const TVec3<double, SIMD::AVX>& v2) -> double
    {
        __m256d z = _mm256_setzero_pd();
        __m256d tv1 = _mm256_blend_pd(v1.v_, z, 0x8);	// = ( 0, z1, y1, x1 )
        __m256d tv2 = _mm256_blend_pd(v2.v_, z, 0x8);	// = ( 0, z2, y2, x2 )
        __m256d t1 = _mm256_mul_pd(tv1, tv2);			// = ( 0, z1 * z2, y1 * y2, x1 * x2 )
        __m256d t2 = _mm256_hadd_pd(t1, t1);			// = ( z1 * z2, z1 * z2, x1 * x2 + y1 * y2, x1 * x2 + y1 * y2 )
        __m128d t3 = _mm256_extractf128_pd(t2, 1);		// = ( z1 * z2, z1 * z2 )
        __m128d t4 = _mm256_castpd256_pd128(t2);		// = ( x1 * x2 + y1 * y2, x1 * x2 + y1 * y2 )
        __m128d result = _mm_add_pd(t3, t4);			// = ( x1 * x2 + y1 * y2 + z1 * z2, x1 * x2 + y1 * y2 + z1 * z2 )
        return _mm_cvtsd_f64(result);
    }
    #endif

    template <typename T, SIMD Opt>
    LM_INLINE auto Dot(const TVec4<T, Opt>& v1, const TVec4<T, Opt>& v2) -> T
    {
        return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z + v1.w * v2.w;
    }

    #if LM_SSE
    template <>
    LM_INLINE auto Dot<float, SIMD::SSE>(const TVec4<float, SIMD::SSE>& v1, const TVec4<float, SIMD::SSE>& v2) -> float
    {
        return _mm_cvtss_f32(_mm_dp_ps(v1.v_, v2.v_, 0xf1));
    }
    #endif

    #if LM_AVX
    template <>
    LM_INLINE auto Dot<double, SIMD::AVX>(const TVec4<double, SIMD::AVX>& v1, const TVec4<double, SIMD::AVX>& v2) -> double
    {
        __m256d t1 = _mm256_mul_pd(v1.v_, v2.v_);   // = ( w1 * w2, z1 * z2, y1 * y2, x1 * x2 )
        __m256d t2 = _mm256_hadd_pd(t1, t1);		// = ( z1 * z2 + w1 * w2, z1 * z2 + w1 * w2, x1 * x2 + y1 * y2, x1 * x2 + y1 * y2 )
        __m128d t3 = _mm256_extractf128_pd(t2, 1);	// = ( z1 * z2 + w1 * w2, z1 * z2 + w1 * w2 )
        __m128d t4 = _mm256_castpd256_pd128(t2);	// = ( x1 * x2 + y1 * y2, x1 * x2 + y1 * y2 )
        __m128d result = _mm_add_pd(t3, t4);		// = ( _, v1.x * v2.x + v1.y * v2.y + v1.z * v2.z + v1.w * v2.w )
        return _mm_cvtsd_f64(result);
    }
    #endif

    #pragma endregion

    // --------------------------------------------------------------------------------

    #pragma region Cross

    template <typename T, SIMD Opt>
    LM_INLINE auto Cross(const TVec3<T, Opt>& v1, const TVec3<T, Opt>& v2) -> TVec3<T, Opt>
    {
        return TVec3<T, Opt>(v1.y * v2.z - v2.y * v1.z, v1.z * v2.x - v2.z * v1.x, v1.x * v2.y - v2.x * v1.y);
    }

    #if LM_SSE
    template <>
    LM_INLINE auto Cross<float, SIMD::SSE>(const TVec3<float, SIMD::SSE>& v1, const TVec3<float, SIMD::SSE>& v2) -> TVec3<float, SIMD::SSE>
    {
        return TVec3<float, SIMD::SSE>(
            _mm_sub_ps(
                _mm_mul_ps(
                    _mm_shuffle_ps(v1.v_, v1.v_, _MM_SHUFFLE(3, 0, 2, 1)),
                    _mm_shuffle_ps(v2.v_, v2.v_, _MM_SHUFFLE(3, 1, 0, 2))),
                _mm_mul_ps(
                    _mm_shuffle_ps(v1.v_, v1.v_, _MM_SHUFFLE(3, 1, 0, 2)),
                    _mm_shuffle_ps(v2.v_, v2.v_, _MM_SHUFFLE(3, 0, 2, 1)))));
    }
    #endif

    #pragma endregion

    // --------------------------------------------------------------------------------

    #pragma region Length2

    template <typename T, SIMD Opt, template <typename, SIMD> class VecT>
    LM_INLINE T Length2(const VecT<T, Opt>& v)
    {
        return Dot(v, v);
    }

    #if LM_SSE
    template <>
    LM_INLINE auto Length2<float, SIMD::SSE, TVec3>(const TVec3<float, SIMD::SSE>& v) -> float
    {
        return _mm_cvtss_f32(_mm_dp_ps(v.v_, v.v_, 0x71));
    }

    template <>
    LM_INLINE auto Length2<float, SIMD::SSE, TVec4>(const TVec4<float, SIMD::SSE>& v) -> float
    {
        return _mm_cvtss_f32(_mm_dp_ps(v.v_, v.v_, 0xf1));
    }
    #endif
 
    #pragma endregion

    // --------------------------------------------------------------------------------

    #pragma region Length

    template <typename T, SIMD Opt, template <typename, SIMD> class VecT>
    LM_INLINE auto Length(const VecT<T, Opt>& v) -> T
    {
        return Math::Sqrt(Math::Length2(v));
    }

    #if LM_SSE
    template <>
    LM_INLINE auto Length<float, SIMD::SSE, TVec3>(const TVec3<float, SIMD::SSE>& v) -> float
    {
        return _mm_cvtss_f32(_mm_sqrt_ss(_mm_dp_ps(v.v_, v.v_, 0x71)));
    }
    #endif

    #if LM_AVX
    template <>
    LM_INLINE auto Length<float, SIMD::SSE, TVec4>(const TVec4<float, SIMD::SSE>& v) -> float
    {
        return _mm_cvtss_f32(_mm_sqrt_ss(_mm_dp_ps(v.v_, v.v_, 0xf1)));
    }
    #endif

    #pragma endregion

    // --------------------------------------------------------------------------------

    #pragma region Normalize

    template <typename T, SIMD Opt, template <typename, SIMD> class VecT>
    LM_INLINE auto Normalize(const VecT<T, Opt>& v) -> VecT<T, Opt>
    {
        return v / Length(v);
    }

    #if LM_SSE
    template <>
    LM_INLINE auto Normalize<float, SIMD::SSE, TVec3>(const TVec3<float, SIMD::SSE>& v) -> TVec3<float, SIMD::SSE>
    {
        return TVec3<float, SIMD::SSE>(_mm_mul_ps(v.v_, _mm_rsqrt_ps(_mm_dp_ps(v.v_, v.v_, 0x7f))));
    }

    template <>
    LM_INLINE auto Normalize<float, SIMD::SSE, TVec4>(const TVec4<float, SIMD::SSE>& v) -> TVec4<float, SIMD::SSE>
    {
        return TVec4<float, SIMD::SSE>(_mm_mul_ps(v.v_, _mm_rsqrt_ps(_mm_dp_ps(v.v_, v.v_, 0xff))));
    }
    #endif

    #pragma endregion

    // --------------------------------------------------------------------------------

    #pragma region Shading coordinates

    template <typename T, SIMD Opt>
    LM_INLINE auto LocalSin(const TVec3<T, Opt>& v) -> T
    {
        return Math::Sqrt(Math::Max(T(0), T(1) - v.z * v.z));
    }

    template <typename T, SIMD Opt>
    LM_INLINE auto LocalCos(const TVec3<T, Opt>& v) -> T
    {
        return v.z;
    }

    template <typename T, SIMD Opt>
    LM_INLINE auto LocalTan(const TVec3<T, Opt>& v) -> T
    {
        const T t = T(1) - v.z * v.z;
        return t <= T(0) ? T(0) : Math::Sqrt<T>(t) / v.z;
    }

    template <typename T, SIMD Opt>
    LM_INLINE auto LocalTan2(const TVec3<T, Opt>& v) -> T
    {
        const T t1 = v.z * v.z;
        const T t2 = T(1) - t1;
        return t2 <= T(0) ? T(0) : t2 / t1;
    }

    #pragma endregion

    // --------------------------------------------------------------------------------

    #pragma region Others

    template <typename T, SIMD Opt>
    LM_INLINE auto IsZero(const TVec3<T, Opt>& v) -> bool
    {
        return v.x == 0_f && v.y == 0_f && v.z == 0_f;
    }

    #if LM_SSE
    template <>
    LM_INLINE auto IsZero<float, SIMD::SSE>(const TVec3<float, SIMD::SSE>& v) -> bool
    {
        return (_mm_movemask_ps(_mm_cmpeq_ps(v.v_, _mm_setzero_ps())) & 0x7) == 7;
    }
    #endif

    // TODO: This should be moved to `Spectrum`
    template <typename T, SIMD Opt>
    LM_INLINE auto Luminance(const TVec3<T, Opt>& v) -> T
    {
        return Math::Dot<T, Opt>(TVec3<T, Opt>(T(0.212671), T(0.715160), T(0.072169)), v);
    }

    #pragma endregion

    #pragma endregion

    // --------------------------------------------------------------------------------

    #pragma region Matrix functions

    #pragma region Transpose

    template <typename T, SIMD Opt>
    LM_INLINE auto Transpose(const TMat3<T, Opt>& m) -> TMat3<T, Opt>
    {
        return TMat3<T, Opt>(
            m[0][0], m[1][0], m[2][0],
            m[0][1], m[1][1], m[2][1],
            m[0][2], m[1][2], m[2][2]);
    }

    template <typename T, SIMD Opt>
    LM_INLINE auto Transpose(const TMat4<T, Opt>& m) -> TMat4<T, Opt>
    {
        return TMat4<T, Opt>(
            m[0][0], m[1][0], m[2][0], m[3][0],
            m[0][1], m[1][1], m[2][1], m[3][1],
            m[0][2], m[1][2], m[2][2], m[3][2],
            m[0][3], m[1][3], m[2][3], m[3][3]);
    }

    #if LM_SSE
    template <>
    LM_INLINE auto Transpose<float, SIMD::SSE>(const TMat4<float, SIMD::SSE>& m) -> TMat4<float, SIMD::SSE>
    {
        __m128 t0 = _mm_unpacklo_ps(m[0].v_, m[1].v_);
        __m128 t2 = _mm_unpacklo_ps(m[2].v_, m[3].v_);
        __m128 t1 = _mm_unpackhi_ps(m[0].v_, m[1].v_);
        __m128 t3 = _mm_unpackhi_ps(m[2].v_, m[3].v_);
        return TMat4<float, SIMD::SSE>(
            _mm_movelh_ps(t0, t2),
            _mm_movehl_ps(t2, t0),
            _mm_movelh_ps(t1, t3),
            _mm_movehl_ps(t3, t1));
    }
    #endif

    #pragma endregion

    // --------------------------------------------------------------------------------

    #pragma region Inverse

    template <typename T, SIMD Opt>
    LM_INLINE auto Inverse(const TMat3<T, Opt>& m) -> TMat3<T, Opt>
    {
        const T det =
              m[0][0] * (m[1][1] * m[2][2] - m[2][1] * m[1][2])
            - m[1][0] * (m[0][1] * m[2][2] - m[2][1] * m[0][2])
            + m[2][0] * (m[0][1] * m[1][2] - m[1][1] * m[0][2]);

        return TMat3<T, Opt>(
            +(m[1][1] * m[2][2] - m[2][1] * m[1][2]),
            -(m[0][1] * m[2][2] - m[2][1] * m[0][2]),
            +(m[0][1] * m[1][2] - m[1][1] * m[0][2]),
            -(m[1][0] * m[2][2] - m[2][0] * m[1][2]),
            +(m[0][0] * m[2][2] - m[2][0] * m[0][2]),
            -(m[0][0] * m[1][2] - m[1][0] * m[0][2]),
            +(m[1][0] * m[2][1] - m[2][0] * m[1][1]),
            -(m[0][0] * m[2][1] - m[2][0] * m[0][1]),
            +(m[0][0] * m[1][1] - m[1][0] * m[0][1])) / det;
    }

    template <typename T, SIMD Opt>
    LM_INLINE auto Inverse(const TMat4<T, Opt>& m) -> TMat4<T, Opt>
    {
        const T c00 = m[2][2] * m[3][3] - m[3][2] * m[2][3];
        const T c02 = m[1][2] * m[3][3] - m[3][2] * m[1][3];
        const T c03 = m[1][2] * m[2][3] - m[2][2] * m[1][3];
        const T c04 = m[2][1] * m[3][3] - m[3][1] * m[2][3];
        const T c06 = m[1][1] * m[3][3] - m[3][1] * m[1][3];
        const T c07 = m[1][1] * m[2][3] - m[2][1] * m[1][3];
        const T c08 = m[2][1] * m[3][2] - m[3][1] * m[2][2];
        const T c10 = m[1][1] * m[3][2] - m[3][1] * m[1][2];
        const T c11 = m[1][1] * m[2][2] - m[2][1] * m[1][2];
        const T c12 = m[2][0] * m[3][3] - m[3][0] * m[2][3];
        const T c14 = m[1][0] * m[3][3] - m[3][0] * m[1][3];
        const T c15 = m[1][0] * m[2][3] - m[2][0] * m[1][3];
        const T c16 = m[2][0] * m[3][2] - m[3][0] * m[2][2];
        const T c18 = m[1][0] * m[3][2] - m[3][0] * m[1][2];
        const T c19 = m[1][0] * m[2][2] - m[2][0] * m[1][2];
        const T c20 = m[2][0] * m[3][1] - m[3][0] * m[2][1];
        const T c22 = m[1][0] * m[3][1] - m[3][0] * m[1][1];
        const T c23 = m[1][0] * m[2][1] - m[2][0] * m[1][1];

        const TVec4<T, Opt> f0(c00, c00, c02, c03);
        const TVec4<T, Opt> f1(c04, c04, c06, c07);
        const TVec4<T, Opt> f2(c08, c08, c10, c11);
        const TVec4<T, Opt> f3(c12, c12, c14, c15);
        const TVec4<T, Opt> f4(c16, c16, c18, c19);
        const TVec4<T, Opt> f5(c20, c20, c22, c23);

        const TVec4<T, Opt> v0(m[1][0], m[0][0], m[0][0], m[0][0]);
        const TVec4<T, Opt> v1(m[1][1], m[0][1], m[0][1], m[0][1]);
        const TVec4<T, Opt> v2(m[1][2], m[0][2], m[0][2], m[0][2]);
        const TVec4<T, Opt> v3(m[1][3], m[0][3], m[0][3], m[0][3]);

        const TVec4<T, Opt> sA(T(+1), T(-1), T(+1), T(-1));
        const TVec4<T, Opt> sB(T(-1), T(+1), T(-1), T(+1));

        const auto inv_v0 = sA * (v1 * f0 - v2 * f1 + v3 * f2);
        const auto inv_v1 = sB * (v0 * f0 - v2 * f3 + v3 * f4);
        const auto inv_v2 = sA * (v0 * f1 - v1 * f3 + v3 * f5);
        const auto inv_v3 = sB * (v0 * f2 - v1 * f4 + v2 * f5);

        const TMat4<T, Opt> inv(inv_v0, inv_v1, inv_v2, inv_v3);
        const T det = Dot(m[0], TVec4<T, Opt>(inv[0][0], inv[1][0], inv[2][0], inv[3][0]));
        const T invDet = 1_f / det;

        return inv * invDet;
    }

    #if LM_SSE
    // cf.
    // http://download.intel.com/design/PentiumIII/sml/24504301.pdf
    // http://devmaster.net/posts/16799/sse-mat4-inverse
    template <>
    LM_INLINE auto Inverse<float, SIMD::SSE>(const TMat4<float, SIMD::SSE>& m) -> TMat4<float, SIMD::SSE>
    {
        __m128 Fac0;
        {
            __m128 Swp0a = _mm_shuffle_ps(m[3].v_, m[2].v_, _MM_SHUFFLE(3, 3, 3, 3));
            __m128 Swp0b = _mm_shuffle_ps(m[3].v_, m[2].v_, _MM_SHUFFLE(2, 2, 2, 2));

            __m128 Swp00 = _mm_shuffle_ps(m[2].v_, m[1].v_, _MM_SHUFFLE(2, 2, 2, 2));
            __m128 Swp01 = _mm_shuffle_ps(Swp0a, Swp0a, _MM_SHUFFLE(2, 0, 0, 0));
            __m128 Swp02 = _mm_shuffle_ps(Swp0b, Swp0b, _MM_SHUFFLE(2, 0, 0, 0));
            __m128 Swp03 = _mm_shuffle_ps(m[2].v_, m[1].v_, _MM_SHUFFLE(3, 3, 3, 3));

            __m128 Mul00 = _mm_mul_ps(Swp00, Swp01);
            __m128 Mul01 = _mm_mul_ps(Swp02, Swp03);
            Fac0 = _mm_sub_ps(Mul00, Mul01);
        }

        __m128 Fac1;
        {
            __m128 Swp0a = _mm_shuffle_ps(m[3].v_, m[2].v_, _MM_SHUFFLE(3, 3, 3, 3));
            __m128 Swp0b = _mm_shuffle_ps(m[3].v_, m[2].v_, _MM_SHUFFLE(1, 1, 1, 1));

            __m128 Swp00 = _mm_shuffle_ps(m[2].v_, m[1].v_, _MM_SHUFFLE(1, 1, 1, 1));
            __m128 Swp01 = _mm_shuffle_ps(Swp0a, Swp0a, _MM_SHUFFLE(2, 0, 0, 0));
            __m128 Swp02 = _mm_shuffle_ps(Swp0b, Swp0b, _MM_SHUFFLE(2, 0, 0, 0));
            __m128 Swp03 = _mm_shuffle_ps(m[2].v_, m[1].v_, _MM_SHUFFLE(3, 3, 3, 3));

            __m128 Mul00 = _mm_mul_ps(Swp00, Swp01);
            __m128 Mul01 = _mm_mul_ps(Swp02, Swp03);
            Fac1 = _mm_sub_ps(Mul00, Mul01);
        }


        __m128 Fac2;
        {
            __m128 Swp0a = _mm_shuffle_ps(m[3].v_, m[2].v_, _MM_SHUFFLE(2, 2, 2, 2));
            __m128 Swp0b = _mm_shuffle_ps(m[3].v_, m[2].v_, _MM_SHUFFLE(1, 1, 1, 1));

            __m128 Swp00 = _mm_shuffle_ps(m[2].v_, m[1].v_, _MM_SHUFFLE(1, 1, 1, 1));
            __m128 Swp01 = _mm_shuffle_ps(Swp0a, Swp0a, _MM_SHUFFLE(2, 0, 0, 0));
            __m128 Swp02 = _mm_shuffle_ps(Swp0b, Swp0b, _MM_SHUFFLE(2, 0, 0, 0));
            __m128 Swp03 = _mm_shuffle_ps(m[2].v_, m[1].v_, _MM_SHUFFLE(2, 2, 2, 2));

            __m128 Mul00 = _mm_mul_ps(Swp00, Swp01);
            __m128 Mul01 = _mm_mul_ps(Swp02, Swp03);
            Fac2 = _mm_sub_ps(Mul00, Mul01);
        }

        __m128 Fac3;
        {
            __m128 Swp0a = _mm_shuffle_ps(m[3].v_, m[2].v_, _MM_SHUFFLE(3, 3, 3, 3));
            __m128 Swp0b = _mm_shuffle_ps(m[3].v_, m[2].v_, _MM_SHUFFLE(0, 0, 0, 0));

            __m128 Swp00 = _mm_shuffle_ps(m[2].v_, m[1].v_, _MM_SHUFFLE(0, 0, 0, 0));
            __m128 Swp01 = _mm_shuffle_ps(Swp0a, Swp0a, _MM_SHUFFLE(2, 0, 0, 0));
            __m128 Swp02 = _mm_shuffle_ps(Swp0b, Swp0b, _MM_SHUFFLE(2, 0, 0, 0));
            __m128 Swp03 = _mm_shuffle_ps(m[2].v_, m[1].v_, _MM_SHUFFLE(3, 3, 3, 3));

            __m128 Mul00 = _mm_mul_ps(Swp00, Swp01);
            __m128 Mul01 = _mm_mul_ps(Swp02, Swp03);
            Fac3 = _mm_sub_ps(Mul00, Mul01);
        }

        __m128 Fac4;
        {
            __m128 Swp0a = _mm_shuffle_ps(m[3].v_, m[2].v_, _MM_SHUFFLE(2, 2, 2, 2));
            __m128 Swp0b = _mm_shuffle_ps(m[3].v_, m[2].v_, _MM_SHUFFLE(0, 0, 0, 0));

            __m128 Swp00 = _mm_shuffle_ps(m[2].v_, m[1].v_, _MM_SHUFFLE(0, 0, 0, 0));
            __m128 Swp01 = _mm_shuffle_ps(Swp0a, Swp0a, _MM_SHUFFLE(2, 0, 0, 0));
            __m128 Swp02 = _mm_shuffle_ps(Swp0b, Swp0b, _MM_SHUFFLE(2, 0, 0, 0));
            __m128 Swp03 = _mm_shuffle_ps(m[2].v_, m[1].v_, _MM_SHUFFLE(2, 2, 2, 2));

            __m128 Mul00 = _mm_mul_ps(Swp00, Swp01);
            __m128 Mul01 = _mm_mul_ps(Swp02, Swp03);
            Fac4 = _mm_sub_ps(Mul00, Mul01);
        }

        __m128 Fac5;
        {
            __m128 Swp0a = _mm_shuffle_ps(m[3].v_, m[2].v_, _MM_SHUFFLE(1, 1, 1, 1));
            __m128 Swp0b = _mm_shuffle_ps(m[3].v_, m[2].v_, _MM_SHUFFLE(0, 0, 0, 0));

            __m128 Swp00 = _mm_shuffle_ps(m[2].v_, m[1].v_, _MM_SHUFFLE(0, 0, 0, 0));
            __m128 Swp01 = _mm_shuffle_ps(Swp0a, Swp0a, _MM_SHUFFLE(2, 0, 0, 0));
            __m128 Swp02 = _mm_shuffle_ps(Swp0b, Swp0b, _MM_SHUFFLE(2, 0, 0, 0));
            __m128 Swp03 = _mm_shuffle_ps(m[2].v_, m[1].v_, _MM_SHUFFLE(1, 1, 1, 1));

            __m128 Mul00 = _mm_mul_ps(Swp00, Swp01);
            __m128 Mul01 = _mm_mul_ps(Swp02, Swp03);
            Fac5 = _mm_sub_ps(Mul00, Mul01);
        }

        __m128 SignA = _mm_set_ps(1.0f, -1.0f, 1.0f, -1.0f);
        __m128 SignB = _mm_set_ps(-1.0f, 1.0f, -1.0f, 1.0f);

        __m128 Temp0 = _mm_shuffle_ps(m[1].v_, m[0].v_, _MM_SHUFFLE(0, 0, 0, 0));
        __m128 Vec0 = _mm_shuffle_ps(Temp0, Temp0, _MM_SHUFFLE(2, 2, 2, 0));

        __m128 Temp1 = _mm_shuffle_ps(m[1].v_, m[0].v_, _MM_SHUFFLE(1, 1, 1, 1));
        __m128 Vec1 = _mm_shuffle_ps(Temp1, Temp1, _MM_SHUFFLE(2, 2, 2, 0));

        __m128 Temp2 = _mm_shuffle_ps(m[1].v_, m[0].v_, _MM_SHUFFLE(2, 2, 2, 2));
        __m128 Vec2 = _mm_shuffle_ps(Temp2, Temp2, _MM_SHUFFLE(2, 2, 2, 0));

        __m128 Temp3 = _mm_shuffle_ps(m[1].v_, m[0].v_, _MM_SHUFFLE(3, 3, 3, 3));
        __m128 Vec3 = _mm_shuffle_ps(Temp3, Temp3, _MM_SHUFFLE(2, 2, 2, 0));

        // col0
        __m128 Mul00 = _mm_mul_ps(Vec1, Fac0);
        __m128 Mul01 = _mm_mul_ps(Vec2, Fac1);
        __m128 Mul02 = _mm_mul_ps(Vec3, Fac2);
        __m128 Sub00 = _mm_sub_ps(Mul00, Mul01);
        __m128 Add00 = _mm_add_ps(Sub00, Mul02);
        __m128 Inv0 = _mm_mul_ps(SignB, Add00);

        // col1
        __m128 Mul03 = _mm_mul_ps(Vec0, Fac0);
        __m128 Mul04 = _mm_mul_ps(Vec2, Fac3);
        __m128 Mul05 = _mm_mul_ps(Vec3, Fac4);
        __m128 Sub01 = _mm_sub_ps(Mul03, Mul04);
        __m128 Add01 = _mm_add_ps(Sub01, Mul05);
        __m128 Inv1 = _mm_mul_ps(SignA, Add01);

        // col2
        __m128 Mul06 = _mm_mul_ps(Vec0, Fac1);
        __m128 Mul07 = _mm_mul_ps(Vec1, Fac3);
        __m128 Mul08 = _mm_mul_ps(Vec3, Fac5);
        __m128 Sub02 = _mm_sub_ps(Mul06, Mul07);
        __m128 Add02 = _mm_add_ps(Sub02, Mul08);
        __m128 Inv2 = _mm_mul_ps(SignB, Add02);

        // col3
        __m128 Mul09 = _mm_mul_ps(Vec0, Fac2);
        __m128 Mul10 = _mm_mul_ps(Vec1, Fac4);
        __m128 Mul11 = _mm_mul_ps(Vec2, Fac5);
        __m128 Sub03 = _mm_sub_ps(Mul09, Mul10);
        __m128 Add03 = _mm_add_ps(Sub03, Mul11);
        __m128 Inv3 = _mm_mul_ps(SignA, Add03);

        __m128 Row0 = _mm_shuffle_ps(Inv0, Inv1, _MM_SHUFFLE(0, 0, 0, 0));
        __m128 Row1 = _mm_shuffle_ps(Inv2, Inv3, _MM_SHUFFLE(0, 0, 0, 0));
        __m128 Row2 = _mm_shuffle_ps(Row0, Row1, _MM_SHUFFLE(2, 0, 2, 0));

        // Determinant
        __m128 Det0 = _mm_dp_ps(m[0].v_, Row2, 0xff);
        __m128 Rcp0 = _mm_div_ps(_mm_set_ps1(1.0f), Det0);

        // Inverse /= Determinant;
        return TMat4<float, SIMD::SSE>(
            TVec4<float, SIMD::SSE>(_mm_mul_ps(Inv0, Rcp0)),
            TVec4<float, SIMD::SSE>(_mm_mul_ps(Inv1, Rcp0)),
            TVec4<float, SIMD::SSE>(_mm_mul_ps(Inv2, Rcp0)),
            TVec4<float, SIMD::SSE>(_mm_mul_ps(Inv3, Rcp0)));
    }
    #endif

    #pragma endregion

    #pragma endregion

    // --------------------------------------------------------------------------------

    #pragma region Transform

    #pragma region Translate

    template <typename T, SIMD Opt>
    LM_INLINE TMat4<T, Opt> Translate(const TMat4<T, Opt>& m, const TVec3<T, Opt>& v)
    {
        TMat4<T, Opt> r(m);
        r[3] = m[0] * v.x + m[1] * v.y + m[2] * v.z + m[3];
        return r;
    }

    template <typename T, SIMD Opt>
    LM_INLINE TMat4<T, Opt> Translate(const TVec3<T, Opt>& v)
    {
        return Translate<T>(TMat4<T, Opt>::Identity(), v);
    }

    #pragma endregion

    // --------------------------------------------------------------------------------

    #pragma region Rotate

    template <typename T, SIMD Opt>
    LM_INLINE TMat4<T, Opt> Rotate(const TMat4<T, Opt>& m, const T& angle, const TVec3<T, Opt>& axis)
    {
	    T c = Cos(angle);
	    T s = Sin(angle);

	    TVec3<T, Opt> a = Normalize(axis);
	    TVec3<T, Opt> t = T(T(1) - c) * a;	// For expression template, (T(1) - c) * a generates compile errors

	    TMat4<T, Opt> rot;
	    rot[0][0] = c + t[0] * a[0];
	    rot[0][1] =     t[0] * a[1] + s * a[2];
	    rot[0][2] =     t[0] * a[2] - s * a[1];
	    rot[1][0] =     t[1] * a[0] - s * a[2];
	    rot[1][1] = c + t[1] * a[1];
	    rot[1][2] =     t[1] * a[2] + s * a[0];
	    rot[2][0] =     t[2] * a[0] + s * a[1];
	    rot[2][1] =     t[2] * a[1] - s * a[0];
	    rot[2][2] = c + t[2] * a[2];

	    TMat4<T, Opt> r;
	    r[0] = m[0] * rot[0][0] + m[1] * rot[0][1] + m[2] * rot[0][2];
	    r[1] = m[0] * rot[1][0] + m[1] * rot[1][1] + m[2] * rot[1][2];
	    r[2] = m[0] * rot[2][0] + m[1] * rot[2][1] + m[2] * rot[2][2];
	    r[3] = m[3];

	    return r;
    }

    template <typename T, SIMD Opt>
    LM_INLINE TMat4<T, Opt> Rotate(const T& angle, const TVec3<T, Opt>& axis)
    {
	    return Rotate(TMat4<T, Opt>::Identity(), angle, axis);
    }

    #pragma endregion

    // --------------------------------------------------------------------------------

    #pragma region Scale

    template <typename T, SIMD Opt>
    LM_INLINE TMat4<T, Opt> Scale(const TMat4<T, Opt>& m, const TVec3<T, Opt>& v)
    {
        return TMat4<T, Opt>(m[0] * v[0], m[1] * v[1], m[2] * v[2], m[3]);
    }

    template <typename T, SIMD Opt>
    LM_INLINE TMat4<T, Opt> Scale(const TVec3<T, Opt>& v)
    {
        return Scale(TMat4<T, Opt>::Identity(), v);
    }

    #pragma endregion

    #pragma endregion   

    // --------------------------------------------------------------------------------

    #pragma region Linear algebra

    template <typename T, SIMD Opt>
    auto OrthonormalBasis(const TVec3<T, Opt>& a, TVec3<T, Opt>& b, TVec3<T, Opt>& c) -> void
    {
        c = Abs(a.x) > Abs(a.y) ? Normalize(Vec3(a.z, 0_f, -a.x)) : Normalize(Vec3(0_f, a.z, -a.y));
        b = Cross(c, a);
        //b = Normalize(Cross(c, a));
    }

    #pragma endregion
}

//! \}
#pragma endregion

LM_NAMESPACE_END
//! \}

