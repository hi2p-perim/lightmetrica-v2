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

#include <lightmetrica/math.h>
#include <limits>
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

template <typename T, typename Enable = void>
struct MathFunc;

template <typename T>
struct MathFunc<T, std::enable_if_t<std::is_arithmetic<T>::value>>
{
    static auto Abs(const T& v) { return std::abs(v); }
};

template <unsigned int Digits>
struct MathFunc<BigFloat<Digits>>
{
    static auto Abs(const BigFloat<Digits>& v) { return boost::multiprecision::abs(v); }
};

namespace
{

    /*
        Checks whether the absolute difference of the two values
        is smaller than the given acceptable error bound.
        Although gtest offers a macro for similar objective (EXPECT_NEAR, etc.),
        we reimplemented this function for we need template-parametrized control
        over various floating point types.
    */
    template <typename T>
    static auto ExpectNear(const T& expected, const T& actual, const T& epsilon = std::numeric_limits<T>::epsilon()) -> ::testing::AssertionResult
    {
        const auto diff = MathFunc<T>::Abs(expected - actual);
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

    template <typename T, SIMD Opt, template <typename, SIMD> class VecT>
    static auto ExpectVecNear(const VecT<T, Opt>& expected, const VecT<T, Opt>& actual, const T& epsilon = std::numeric_limits<T>::epsilon()) -> ::testing::AssertionResult
    {
        constexpr int N = VecT<T, Opt>::NC;
        for (int i = 0; i < N; i++)
        {
            auto result = ExpectNear(expected[i], actual[i], epsilon);
            if (!result)
            {
                result << ", column " << i;
                return result;
            }
        }

        return ::testing::AssertionSuccess();
    }

    template <typename T, SIMD Opt, template <typename, SIMD> class MatT>
    static auto ExpectMatNear(const MatT<T, Opt>& expected, const MatT<T, Opt>& actual, const T& epsilon = std::numeric_limits<T>::epsilon()) -> ::testing::AssertionResult
    {
        constexpr int N = MatT<T, Opt>::NC;
        for (int i = 0; i < N; i++)
        {
            for (int j = 0; j < N; j++)
            {
                auto result = ExpectNear(expected[i][j], actual[i][j], epsilon);
                if (!result)
                {
                    result << ", row " << i << ", column " << j;
                    return result;
                }
            }
        }

        return ::testing::AssertionSuccess();
    }

}

#pragma endregion

LM_TEST_NAMESPACE_END
