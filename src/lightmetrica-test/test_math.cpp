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
    static auto ExpectNear(const T& expected, const T& actual, const T& epsilon) -> ::testing::AssertionResult
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
    static auto ExpectNear(const BigFloat<Digits>& expected, const BigFloat<Digits>& actual, const BigFloat<Digits>& epsilon) -> ::testing::AssertionResult
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

#pragma region Vector type

template <typename T_, SIMD Opt_>
struct VecTypeParam
{
    using T = typename T_;
    static constexpr SIMD Opt = Opt_;
};

template <typename T>
struct Vec3Test : public ::testing::Test {};

using VecTestTypes = ::testing::Types<
    VecTypeParam<float, SIMD::None>,
    VecTypeParam<float, SIMD::SSE>,
    VecTypeParam<double, SIMD::AVX>,
    VecTypeParam<double, SIMD::None>,
    VecTypeParam<BigFloat50, SIMD::None>,
    VecTypeParam<BigFloat100, SIMD::None>
>;

TYPED_TEST_CASE(Vec3Test, VecTestTypes);

TYPED_TEST(Vec3Test, DefaultConstructor)
{
    using T = TypeParam;
    TVec3<T::T, T::Opt> v;
    EXPECT_EQ(0, v.x);
    EXPECT_EQ(0, v.y);
    EXPECT_EQ(0, v.z);
}

TYPED_TEST(Vec3Test, Constructor)
{
    using T = TypeParam::T;
    const auto eps = std::numeric_limits<T>::epsilon();
    TVec3<T, TypeParam::Opt> v(T(1), T(2), T(3));
    EXPECT_TRUE(MathTestUtils<T>::ExpectNear(T(1), v.x, eps));
    EXPECT_TRUE(MathTestUtils<T>::ExpectNear(T(2), v.y, eps));
    EXPECT_TRUE(MathTestUtils<T>::ExpectNear(T(3), v.z, eps));
}

#pragma endregion

// --------------------------------------------------------------------------------

LM_TEST_NAMESPACE_END
