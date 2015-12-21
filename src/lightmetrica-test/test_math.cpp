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

#include <pch_test.h>
#include <lightmetrica/math.h>
#include <boost/multiprecision/cpp_dec_float.hpp>

LM_TEST_NAMESPACE_BEGIN

#pragma region Multi-precision types

/*
    We add tests for multi-precision floating point number
    with boost::multiprecision.
*/
template <unsigned int Digits>
using BigFloat = boost::multiprecision::number<boost::multiprecision::cpp_dec_float<Digits>>;
using BigFloat50  = BigFloat<50>;
using BigFloat100 = BigFloat<100>;

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Comparison helper for floating point types

/*
    Checks whether the absolute difference of the two values
    is smaller than the given acceptable error bound.
    Although gtest offers a macro for similar objective (EXPECT_NEAR, etc.),
    we reimplemented this function for we need template-parametrized control
    over various floating point types.
*/
template <typename T, typename Enable = void>
struct MathTestUtils;

template <typename T>
struct MathTestUtils<
    T,
    std::enable_if_t<
        std::is_same<T, float>::value || std::is_same<T, double>::value
    >
>
{
    static auto ExpectNear(const T& expected, const T& actual, const T& epsilon = std::numeric_limits<T>::epsilon()) -> ::testing::AssertionResult
    {
        const auto diff = std::abs<T>(expected - actual);
        if (diff > epsilon)
        {
            return ::testing::AssertionFailure()
                << "Expected " << expected << ", "
                << "Actual " << actual << ", "
                << "Diff " << diff << ", "
                << "Epsilon " << epsilon;
        }

        return ::testing::AssertionSuccess();
    }
};

template <unsigned int Digits>
struct MathTestUtils<BigFloat<Digits>>
{
    using VT = BigFloat<Digits>;
    static auto ExpectNear(const VT& expected, const VT& actual, const VT& epsilon = std::numeric_limits<VT>::epsilon()) -> ::testing::AssertionResult
    {
        const auto diff = boost::multiprecision::abs(expected - actual);
        if (diff > epsilon)
        {
            return ::testing::AssertionFailure()
                << "Expected " << expected << ", "
                << "Actual " << actual << ", "
                << "Diff " << diff << ", "
                << "Epsilon " << epsilon;
        }

        return ::testing::AssertionSuccess();
    }
};

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Tests for vector types (Vec2, Vec3, Vec4)

template <typename T_, SIMD Opt_>
struct VecTParam
{
    using T = typename T_;
    static constexpr SIMD Opt = Opt_;
};

using VecTestTypes = ::testing::Types<
    VecTParam<float, SIMD::None>,
    VecTParam<float, SIMD::SSE>,
    VecTParam<double, SIMD::AVX>,
    VecTParam<double, SIMD::None>,
    VecTParam<BigFloat50, SIMD::None>,
    VecTParam<BigFloat100, SIMD::None>
>;

// --------------------------------------------------------------------------------

#pragma region Vec3

template <typename T>
struct Vec3Test : public ::testing::Test {};

TYPED_TEST_CASE(Vec3Test, VecTestTypes);

TYPED_TEST(Vec3Test, DefaultConstructor)
{
    using T = TypeParam::T;
    using VecT = TVec3<T, TypeParam::Opt>;
    VecT v;
    EXPECT_EQ(0, v.x);
    EXPECT_EQ(0, v.y);
    EXPECT_EQ(0, v.z);
}

TYPED_TEST(Vec3Test, Constructor)
{
    using T = TypeParam::T;
    using VecT = TVec3<T, TypeParam::Opt>;
    VecT v(T(1), T(2), T(3));
    EXPECT_TRUE(MathTestUtils<T>::ExpectNear(T(1), v.x));
    EXPECT_TRUE(MathTestUtils<T>::ExpectNear(T(2), v.y));
    EXPECT_TRUE(MathTestUtils<T>::ExpectNear(T(3), v.z));
}

TYPED_TEST(Vec3Test, Accessor1)
{
    using T = TypeParam::T;
    using VecT = TVec3<T, TypeParam::Opt>;
    VecT v(T(1), T(2), T(3));
    EXPECT_TRUE(MathTestUtils<T>::ExpectNear(T(1), v[0]));
    EXPECT_TRUE(MathTestUtils<T>::ExpectNear(T(2), v[1]));
    EXPECT_TRUE(MathTestUtils<T>::ExpectNear(T(3), v[2]));
}

TYPED_TEST(Vec3Test, Accessor2)
{
    using T = TypeParam::T;
    using VecT = TVec3<T, TypeParam::Opt>;
    VecT v;
    v[0] = 1;
    v[1] = 2;
    v[2] = 3;
    EXPECT_TRUE(MathTestUtils<T>::ExpectNear(T(1), v.x));
    EXPECT_TRUE(MathTestUtils<T>::ExpectNear(T(2), v.y));
    EXPECT_TRUE(MathTestUtils<T>::ExpectNear(T(3), v.z));
}

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Vec4

template <typename T>
struct Vec4Test : public ::testing::Test {};

TYPED_TEST_CASE(Vec4Test, VecTestTypes);

TYPED_TEST(Vec4Test, DefaultConstructor)
{
    using T = TypeParam::T;
    using VecT = TVec4<T, TypeParam::Opt>;
    VecT v;
    EXPECT_EQ(0, v.x);
    EXPECT_EQ(0, v.y);
    EXPECT_EQ(0, v.z);
    EXPECT_EQ(0, v.w);
}

TYPED_TEST(Vec4Test, Constructor)
{
    using T = TypeParam::T;
    using VecT = TVec4<T, TypeParam::Opt>;
    VecT v(T(1), T(2), T(3), T(4));
    EXPECT_TRUE(MathTestUtils<T>::ExpectNear(T(1), v.x));
    EXPECT_TRUE(MathTestUtils<T>::ExpectNear(T(2), v.y));
    EXPECT_TRUE(MathTestUtils<T>::ExpectNear(T(3), v.z));
    EXPECT_TRUE(MathTestUtils<T>::ExpectNear(T(4), v.w));
}

TYPED_TEST(Vec4Test, Accessor1)
{
    using T = TypeParam::T;
    using VecT = TVec4<T, TypeParam::Opt>;
    VecT v(T(1), T(2), T(3), T(4));
    EXPECT_TRUE(MathTestUtils<T>::ExpectNear(T(1), v[0]));
    EXPECT_TRUE(MathTestUtils<T>::ExpectNear(T(2), v[1]));
    EXPECT_TRUE(MathTestUtils<T>::ExpectNear(T(3), v[2]));
    EXPECT_TRUE(MathTestUtils<T>::ExpectNear(T(4), v[3]));
}

TYPED_TEST(Vec4Test, Accessor2)
{
    using T = TypeParam::T;
    using VecT = TVec4<T, TypeParam::Opt>;
    VecT v;
    v[0] = 1;
    v[1] = 2;
    v[2] = 3;
    v[3] = 4;
    EXPECT_TRUE(MathTestUtils<T>::ExpectNear(T(1), v.x));
    EXPECT_TRUE(MathTestUtils<T>::ExpectNear(T(2), v.y));
    EXPECT_TRUE(MathTestUtils<T>::ExpectNear(T(3), v.z));
    EXPECT_TRUE(MathTestUtils<T>::ExpectNear(T(4), v.w));
}

#pragma endregion

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Tests for vector operations

template <typename T_, SIMD Opt_, template <typename, SIMD> class VecT_>
struct VecOpParam
{
    using T = typename T_;
    static constexpr SIMD Opt = Opt_;
    using VecT = typename VecT_<T_, Opt_>;
};

template <typename T>
struct VecOpTest : public ::testing::Test {};

using VecOpTestTypes = ::testing::Types<
    VecOpParam<float, SIMD::None, TVec3>,
    VecOpParam<float, SIMD::SSE, TVec3>,
    VecOpParam<double, SIMD::AVX, TVec3>,
    VecOpParam<double, SIMD::None, TVec3>,
    VecOpParam<BigFloat50, SIMD::None, TVec3>,
    VecOpParam<BigFloat100, SIMD::None, TVec3>,
    VecOpParam<float, SIMD::None, TVec4>,
    VecOpParam<float, SIMD::SSE, TVec4>,
    VecOpParam<double, SIMD::AVX, TVec4>,
    VecOpParam<double, SIMD::None, TVec4>,
    VecOpParam<BigFloat50, SIMD::None, TVec4>,
    VecOpParam<BigFloat100, SIMD::None, TVec4>
>;

TYPED_TEST_CASE(VecOpTest, VecOpTestTypes);

TYPED_TEST(VecOpTest, Add)
{
    using T = TypeParam::T;
    using VecT = TypeParam::VecT;
    constexpr int N = VecT::NC;

    T v1i[] = { T(1), T(2), T(3), T(4) };
    T v2i[] = { T(4), T(3), T(2), T(1) };
    T ans[] = { T(5), T(5), T(5), T(5) };

    VecT v1;
    VecT v2;
    for (int i = 0; i < N; i++)
    {
        v1[i] = v1i[i];
        v2[i] = v2i[i];
    }

    const auto result = v1 + v2;
    for (int i = 0; i < N; i++)
    {
        EXPECT_TRUE(MathTestUtils<T>::ExpectNear(ans[i], result[i]));
    }
}

TYPED_TEST(VecOpTest, Subtract)
{
    using T = TypeParam::T;
    using VecT = TypeParam::VecT;
    constexpr int N = VecT::NC;

    T v1i[] = { T(1), T(2), T(3), T(4) };
    T v2i[] = { T(4), T(3), T(2), T(1) };
    T ans[] = { T(-3), T(-1), T(1), T(3) };

    VecT v1;
    VecT v2;
    for (int i = 0; i < N; i++)
    {
        v1[i] = v1i[i];
        v2[i] = v2i[i];
    }

    const auto result = v1 - v2;
    for (int i = 0; i < N; i++)
    {
        EXPECT_TRUE(MathTestUtils<T>::ExpectNear(ans[i], result[i]));
    }
}

TYPED_TEST(VecOpTest, Multiply)
{
    using T = TypeParam::T;
    using VecT = TypeParam::VecT;
    constexpr int N = VecT::NC;

    T v1i[] = { T(1), T(2), T(3), T(4) };
    T v2i[] = { T(4), T(3), T(2), T(1) };
    T ans[] = { T(4), T(6), T(6), T(4) };

    VecT v1;
    VecT v2;
    for (int i = 0; i < N; i++)
    {
        v1[i] = v1i[i];
        v2[i] = v2i[i];
    }

    const auto result = v1 * v2;
    for (int i = 0; i < N; i++)
    {
        EXPECT_TRUE(MathTestUtils<T>::ExpectNear(ans[i], result[i]));
    }
}

TYPED_TEST(VecOpTest, Divide)
{
    using T = TypeParam::T;
    using VecT = TypeParam::VecT;
    constexpr int N = VecT::NC;

    T v1i[] = { T(12), T(12), T(12), T(12) };
    T v2i[] = { T(2), T(3), T(4), T(6) };
    T ans[] = { T(6), T(4), T(3), T(2) };

    VecT v1;
    VecT v2;
    for (int i = 0; i < N; i++)
    {
        v1[i] = v1i[i];
        v2[i] = v2i[i];
    }

    const auto result = v1 / v2;
    for (int i = 0; i < N; i++)
    {
        EXPECT_TRUE(MathTestUtils<T>::ExpectNear(ans[i], result[i]));
    }
}

TYPED_TEST(VecOpTest, AddAssign)
{
    using T = TypeParam::T;
    using VecT = TypeParam::VecT;
    constexpr int N = VecT::NC;

    T v1i[] = { T(1), T(2), T(3), T(4) };
    T v2i[] = { T(4), T(3), T(2), T(1) };
    T ans[] = { T(5), T(5), T(5), T(5) };

    VecT v1;
    VecT v2;
    for (int i = 0; i < N; i++)
    {
        v1[i] = v1i[i];
        v2[i] = v2i[i];
    }

    v1 += v2;
    for (int i = 0; i < N; i++)
    {
        EXPECT_TRUE(MathTestUtils<T>::ExpectNear(ans[i], v1[i]));
    }
}

TYPED_TEST(VecOpTest, SubtractAssign)
{
    using T = TypeParam::T;
    using VecT = TypeParam::VecT;
    constexpr int N = VecT::NC;

    T v1i[] = { T(1), T(2), T(3), T(4) };
    T v2i[] = { T(4), T(3), T(2), T(1) };
    T ans[] = { T(-3), T(-1), T(1), T(3) };

    VecT v1;
    VecT v2;
    for (int i = 0; i < N; i++)
    {
        v1[i] = v1i[i];
        v2[i] = v2i[i];
    }

    v1 -= v2;
    const auto result = v1 - v2;
    for (int i = 0; i < N; i++)
    {
        EXPECT_TRUE(MathTestUtils<T>::ExpectNear(ans[i], v1[i]));
    }
}

TYPED_TEST(VecOpTest, MultiplyAssign)
{
    using T = TypeParam::T;
    using VecT = TypeParam::VecT;
    constexpr int N = VecT::NC;

    T v1i[] = { T(1), T(2), T(3), T(4) };
    T v2i[] = { T(4), T(3), T(2), T(1) };
    T ans[] = { T(4), T(6), T(6), T(4) };

    VecT v1;
    VecT v2;
    for (int i = 0; i < N; i++)
    {
        v1[i] = v1i[i];
        v2[i] = v2i[i];
    }

    v1 *= v2;
    for (int i = 0; i < N; i++)
    {
        EXPECT_TRUE(MathTestUtils<T>::ExpectNear(ans[i], v1[i]));
    }
}

TYPED_TEST(VecOpTest, DivideAssign)
{
    using T = TypeParam::T;
    using VecT = TypeParam::VecT;
    constexpr int N = VecT::NC;

    T v1i[] = { T(12), T(12), T(12), T(12) };
    T v2i[] = { T(2), T(3), T(4), T(6) };
    T ans[] = { T(6), T(4), T(3), T(2) };

    VecT v1;
    VecT v2;
    for (int i = 0; i < N; i++)
    {
        v1[i] = v1i[i];
        v2[i] = v2i[i];
    }

    v1 /= v2;
    for (int i = 0; i < N; i++)
    {
        EXPECT_TRUE(MathTestUtils<T>::ExpectNear(ans[i], v1[i]));
    }
}

#pragma endregion

// --------------------------------------------------------------------------------

LM_TEST_NAMESPACE_END
