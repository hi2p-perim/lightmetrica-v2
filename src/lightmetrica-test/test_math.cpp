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
#include <lightmetrica-test/mathutils.h>

LM_TEST_NAMESPACE_BEGIN

// --------------------------------------------------------------------------------

#pragma region Test types

template <typename T_, SIMD Opt_>
struct TParam
{
    using T = typename T_;
    static constexpr SIMD Opt = Opt_;
};

using TestTypes = ::testing::Types<
    TParam<float, SIMD::None>,
    TParam<float, SIMD::SSE>,
    TParam<double, SIMD::AVX>,
    TParam<double, SIMD::None>,
    TParam<BigFloat50, SIMD::None>,
    TParam<BigFloat100, SIMD::None>
>;

template <typename T_, SIMD Opt_, template <typename, SIMD> class VecT_>
struct OpVecTParam
{
    using T = typename T_;
    static constexpr SIMD Opt = Opt_;
    
    template <typename T__, SIMD Opt__>
    using TVec = VecT_<T__, Opt__>;
    using VecT = TVec<T, Opt>;
};

using VecOpTestTypes = ::testing::Types<
    OpVecTParam<float, SIMD::None, TVec3>,
    OpVecTParam<float, SIMD::SSE, TVec3>,
    OpVecTParam<double, SIMD::AVX, TVec3>,
    OpVecTParam<double, SIMD::None, TVec3>,
    OpVecTParam<BigFloat50, SIMD::None, TVec3>,
    OpVecTParam<BigFloat100, SIMD::None, TVec3>,
    OpVecTParam<float, SIMD::None, TVec4>,
    OpVecTParam<float, SIMD::SSE, TVec4>,
    OpVecTParam<double, SIMD::AVX, TVec4>,
    OpVecTParam<double, SIMD::None, TVec4>,
    OpVecTParam<BigFloat50, SIMD::None, TVec4>,
    OpVecTParam<BigFloat100, SIMD::None, TVec4>
>;

template <typename T_, SIMD Opt_, template <typename, SIMD> class MatT_>
struct OpMatTParam
{
    using T = typename T_;
    static constexpr SIMD Opt = Opt_;

    template <typename T__, SIMD Opt__>
    using TMat = MatT_<T__, Opt__>;
    using MatT = TMat<T, Opt>;
    
    using VecT = typename TMat<T, Opt>::VecT;
};

using MatOpTestTypes = ::testing::Types<
    OpMatTParam<float, SIMD::None, TMat3>,
    OpMatTParam<float, SIMD::SSE, TMat3>,
    OpMatTParam<double, SIMD::AVX, TMat3>,
    OpMatTParam<double, SIMD::None, TMat3>,
    OpMatTParam<BigFloat50, SIMD::None, TMat3>,
    OpMatTParam<BigFloat100, SIMD::None, TMat3>,
    OpMatTParam<float, SIMD::None, TMat4>,
    OpMatTParam<float, SIMD::SSE, TMat4>,
    OpMatTParam<double, SIMD::AVX, TMat4>,
    OpMatTParam<double, SIMD::None, TMat4>,
    OpMatTParam<BigFloat50, SIMD::None, TMat4>,
    OpMatTParam<BigFloat100, SIMD::None, TMat4>
>;

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Tests for vector types (Vec3, Vec4)

#pragma region Vec3

template <typename T>
struct Vec3Test : public ::testing::Test {};

TYPED_TEST_CASE(Vec3Test, TestTypes);

TYPED_TEST(Vec3Test, DefaultConstructor)
{
    using T = TypeParam::T;
    using VecT = TVec3<T, TypeParam::Opt>;
    VecT v;
    EXPECT_EQ(0, v.x);
    EXPECT_EQ(0, v.y);
    EXPECT_EQ(0, v.z);
}

TYPED_TEST(Vec3Test, Constructor1)
{
    using T = TypeParam::T;
    using VecT = TVec3<T, TypeParam::Opt>;
    VecT v(T(1), T(2), T(3));
    EXPECT_TRUE(ExpectNear(T(1), v.x));
    EXPECT_TRUE(ExpectNear(T(2), v.y));
    EXPECT_TRUE(ExpectNear(T(3), v.z));
}

TYPED_TEST(Vec3Test, Constructor2)
{
    using T = TypeParam::T;
    using VecT = TVec4<T, TypeParam::Opt>;
    VecT v{1,2,3};
    EXPECT_TRUE(ExpectNear(T(1), v.x));
    EXPECT_TRUE(ExpectNear(T(2), v.y));
    EXPECT_TRUE(ExpectNear(T(3), v.z));
}

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Vec4

template <typename T>
struct Vec4Test : public ::testing::Test {};

TYPED_TEST_CASE(Vec4Test, TestTypes);

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

TYPED_TEST(Vec4Test, Constructor1)
{
    using T = TypeParam::T;
    using VecT = TVec4<T, TypeParam::Opt>;
    VecT v(T(1), T(2), T(3), T(4));
    EXPECT_TRUE(ExpectNear(T(1), v.x));
    EXPECT_TRUE(ExpectNear(T(2), v.y));
    EXPECT_TRUE(ExpectNear(T(3), v.z));
    EXPECT_TRUE(ExpectNear(T(4), v.w));
}

TYPED_TEST(Vec4Test, Constructor2)
{
    using T = TypeParam::T;
    using VecT = TVec4<T, TypeParam::Opt>;
    VecT v{1,2,3,4};
    EXPECT_TRUE(ExpectNear(T(1), v.x));
    EXPECT_TRUE(ExpectNear(T(2), v.y));
    EXPECT_TRUE(ExpectNear(T(3), v.z));
    EXPECT_TRUE(ExpectNear(T(4), v.w));
}

#pragma endregion

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Tests for vector operations

template <typename T>
struct VecOpTest : public ::testing::Test {};

TYPED_TEST_CASE(VecOpTest, VecOpTestTypes);

TYPED_TEST(VecOpTest, Accessor1)
{
    using T = TypeParam::T;
    TypeParam::VecT v1{1,2,3,4};
    const T v2[] = {1,2,3,4};
    for (int i = 0; i < TypeParam::VecT::NC; i++)
    {
        EXPECT_TRUE(ExpectNear(v2[i], v1[i]));
    }
}

TYPED_TEST(VecOpTest, Accessor2)
{
    using T = TypeParam::T;
    TypeParam::VecT v1{ 1,2,3,4 };
    const T v2[] = { 1,2,3,4 };
    for (int i = 0; i < TypeParam::VecT::NC; i++)
    {
        v1[i] = v2[i];
        EXPECT_TRUE(ExpectNear(v2[i], v1[i]));
    }
}

TYPED_TEST(VecOpTest, Add)
{
    TypeParam::VecT v1{1,2,3,4};
    TypeParam::VecT v2{4,3,2,1};
    TypeParam::VecT v3{5,5,5,5};
    const auto r = ExpectVecNear(v3, v1 + v2);
    EXPECT_TRUE(r);
}

TYPED_TEST(VecOpTest, Subtract)
{
    TypeParam::VecT v1{1,2,3,4};
    TypeParam::VecT v2{4,3,2,1};
    TypeParam::VecT v3{-3,-1,1,3};
    const auto r = ExpectVecNear(v3, v1 - v2);
    EXPECT_TRUE(r);
}

TYPED_TEST(VecOpTest, Multiply)
{
    TypeParam::VecT v1{1,2,3,4};
    TypeParam::VecT v2{4,3,2,1};
    TypeParam::VecT v3{4,6,6,4};
    const auto r = ExpectVecNear(v3, v1 * v2);
    EXPECT_TRUE(r);
}

TYPED_TEST(VecOpTest, Divide)
{
    TypeParam::VecT v1{12,12,12,12};
    TypeParam::VecT v2{2,3,4,6};
    TypeParam::VecT v3{6,4,3,2};
    const auto r = ExpectVecNear(v3, v1 / v2);
    EXPECT_TRUE(r);
}

TYPED_TEST(VecOpTest, AddAssign)
{
    TypeParam::VecT v1{1,2,3,4};
    TypeParam::VecT v2{4,3,2,1};
    TypeParam::VecT v3{5,5,5,5};
    v1 += v2;
    const auto r = ExpectVecNear(v3, v1);
    EXPECT_TRUE(r);
}

TYPED_TEST(VecOpTest, SubtractAssign)
{
    TypeParam::VecT v1{1,2,3,4};
    TypeParam::VecT v2{4,3,2,1};
    TypeParam::VecT v3{-3,-1,1,3};
    v1 -= v2;
    const auto r = ExpectVecNear(v3, v1);
    EXPECT_TRUE(r);
}

TYPED_TEST(VecOpTest, MultiplyAssign)
{
    TypeParam::VecT v1{1,2,3,4};
    TypeParam::VecT v2{4,3,2,1};
    TypeParam::VecT v3{4,6,6,4};
    v1 *= v2;
    const auto r = ExpectVecNear(v3, v1);
    EXPECT_TRUE(r);
}

TYPED_TEST(VecOpTest, DivideAssign)
{
    TypeParam::VecT v1{12,12,12,12};
    TypeParam::VecT v2{2,3,4,6};
    TypeParam::VecT v3{6,4,3,2};
    v1 /= v2;
    const auto r = ExpectVecNear(v3, v1);
    EXPECT_TRUE(r);
}

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Tests for matrix types (Mat3, Mat4)

#pragma region Mat3

template <typename T>
struct Mat3Test : public ::testing::Test {};

TYPED_TEST_CASE(Mat3Test, TestTypes);

TYPED_TEST(Mat3Test, DefaultConstructor)
{
    using T = TypeParam::T;
    using MatT = TMat3<T, TypeParam::Opt>;
    MatT v;
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            EXPECT_EQ(T(0), v.v_[i][j]);
        }
    }
}

TYPED_TEST(Mat3Test, Constructor1)
{
    using T = TypeParam::T;
    using MatT = TMat3<T, TypeParam::Opt>;

    MatT m(T(1), T(2), T(3),
           T(4), T(5), T(6),
           T(7), T(8), T(9));

    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            int ind = i * 3 + j + 1;
            EXPECT_EQ(T(ind), m.v_[i][j]);
        }
    }
}

TYPED_TEST(Mat3Test, Constructor2)
{
    using T = TypeParam::T;
    using MatT = TMat3<T, TypeParam::Opt>;

    MatT m{1,2,3,4,5,6,7,8,9};

    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            int ind = i * 3 + j + 1;
            EXPECT_EQ(T(ind), m.v_[i][j]);
        }
    }
}

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Mat4

template <typename T>
struct Mat4Test : public ::testing::Test {};

TYPED_TEST_CASE(Mat4Test, TestTypes);

TYPED_TEST(Mat4Test, DefaultConstructor)
{
    using T = TypeParam::T;
    using MatT = TMat4<T, TypeParam::Opt>;
    MatT v;
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            EXPECT_EQ(T(0), v.v_[i][j]);
        }
    }
}

TYPED_TEST(Mat4Test, Constructor1)
{
    using T = TypeParam::T;
    using MatT = TMat4<T, TypeParam::Opt>;

    MatT m(T(1), T(2), T(3), T(4),
           T(5), T(6), T(7), T(8),
           T(9), T(10), T(11), T(12),
           T(13), T(14), T(15), T(16));

    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            int ind = i * 4 + j + 1;
            EXPECT_EQ(T(ind), m.v_[i][j]);
        }
    }
}

TYPED_TEST(Mat4Test, Constructor2)
{
    using T = TypeParam::T;
    using MatT = TMat4<T, TypeParam::Opt>;

    MatT m{1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};

    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            int ind = i * 4 + j + 1;
            EXPECT_EQ(T(ind), m.v_[i][j]);
        }
    }
}

#pragma endregion

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Tests for matrix operations

template <typename T>
struct MatOpTest : public ::testing::Test {};

TYPED_TEST_CASE(MatOpTest, MatOpTestTypes);

TYPED_TEST(MatOpTest, Accessor1)
{
    using T = TypeParam::T;
    constexpr int N = TypeParam::MatT::NC;
    TypeParam::MatT m1{1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
    const T m2[] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
    for (int i = 0; i < N; i++)
    {
        for (int j = 0; j < N; j++)
        {
            EXPECT_TRUE(ExpectNear(m2[i*N+j], m1[i][j]));
        }
    }
}

TYPED_TEST(MatOpTest, Accessor2)
{
    using T = TypeParam::T;
    constexpr int N = TypeParam::MatT::NC;
    TypeParam::MatT m1{1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
    const T m2[] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
    for (int i = 0; i < N; i++)
    {
        for (int j = 0; j < N; j++)
        {
            m1[i][j] = m2[i*N+j];
            EXPECT_TRUE(ExpectNear(m2[i*N+j], m1[i][j]));
        }
    }
}

TYPED_TEST(MatOpTest, Multiply)
{
    TypeParam::MatT m1{
        1,1,1,1,
        1,1,1,1,
        1,1,1,1,
        1,1,1,1};
    TypeParam::MatT m2{
        1,1,1,1,
        1,1,1,1,
        1,1,1,1,
        1,1,1,1};
    const TypeParam::T t(TypeParam::MatT::NC);
    TypeParam::MatT m3{
        t,t,t,t,
        t,t,t,t,
        t,t,t,t,
        t,t,t,t};
    const auto r = ExpectMatNear(m3, m1 * m2);
    EXPECT_TRUE(r);
}

TYPED_TEST(MatOpTest, MultiplyVector)
{
    TypeParam::MatT m{
        1,1,1,1,
        1,1,1,1,
        1,1,1,1,
        1,1,1,1 };
    TypeParam::VecT v1{1,1,1,1};
    const TypeParam::T t(TypeParam::MatT::NC);
    TypeParam::VecT v2{t,t,t,t};
    const auto r = ExpectVecNear(v2, m * v1);
    EXPECT_TRUE(r);
}

#pragma endregion

LM_TEST_NAMESPACE_END
