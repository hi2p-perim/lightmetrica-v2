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
#include <lightmetrica/test.h>

LM_TEST_NAMESPACE_BEGIN

// --------------------------------------------------------------------------------

struct E : public Component
{
    LM_INTERFACE_CLASS(E, Component, 1);
    LM_INTERFACE_F(0, Func, int());
};

struct E1 : public E
{
    LM_IMPL_CLASS(E1, E);
    LM_IMPL_F(Func, [this]() -> int { return 42; });
};

LM_COMPONENT_REGISTER_IMPL(E1);

TEST(ComponentTest, E)
{
    auto p = std::move(ComponentFactory::Create<E>("E1"));
    EXPECT_EQ(42, p->Func());
}

// --------------------------------------------------------------------------------

TEST(ComponentTest, A)
{
    auto p = std::move(ComponentFactory::Create<A>("A1"));

    ASSERT_FALSE(p == nullptr);

    {
        testing::internal::CaptureStdout();
        p->Func1(42);
        const auto out = testing::internal::GetCapturedStdout();
        EXPECT_EQ("42\n", out);
    }

    EXPECT_EQ(3, p->Func2(1, 2));

    {
        testing::internal::CaptureStdout();
        p->Func3();
        const auto out = testing::internal::GetCapturedStdout();
        EXPECT_EQ("hello\n", out);
    }
}

TEST(ComponentTest, FailedToCreate)
{
    auto* p = ComponentFactory::Create("A2");
    ASSERT_TRUE(p == nullptr);
}

TEST(ComponentTest, B)
{
    auto p = std::move(ComponentFactory::Create<B>("B1"));

    ASSERT_FALSE(p == nullptr);

    {
        testing::internal::CaptureStdout();
        p->Func1(42);
        const auto out = testing::internal::GetCapturedStdout();
        EXPECT_EQ("43\n", out);
    }

    EXPECT_EQ(4, p->Func2(1, 2));

    {
        testing::internal::CaptureStdout();
        p->Func3();
        const auto out = testing::internal::GetCapturedStdout();
        EXPECT_EQ("a\n", out);
    }

    {
        testing::internal::CaptureStdout();
        p->Func4();
        const auto out = testing::internal::GetCapturedStdout();
        EXPECT_EQ("b\n", out);
    }
}

TEST(ComponentTest, C)
{
    auto p = std::move(ComponentFactory::Create<C>("C1"));

    std::vector<int> v{ 1,2,3 };
    
    {
        testing::internal::CaptureStdout();
        p->Func1(v.data(), 3);
        const auto out = testing::internal::GetCapturedStdout();
        EXPECT_EQ("1 2 3 \n", out);
    }

    {
        testing::internal::CaptureStdout();
        p->Func2(v);
        const auto out = testing::internal::GetCapturedStdout();
        EXPECT_EQ("1 2 3 \n", out);
    }

    {
        int t;
        p->Func3(t);
        EXPECT_EQ(42, t);
    }

    {
        int t = 42;
        testing::internal::CaptureStdout();
        p->Func4(t);
        const auto out = testing::internal::GetCapturedStdout();
        EXPECT_EQ("42\n", out);
    }

    {
        std::string str = "hello";
        testing::internal::CaptureStdout();
        p->Func5(str);
        const auto out = testing::internal::GetCapturedStdout();
        EXPECT_EQ("hello\n", out);
    }
}

LM_TEST_NAMESPACE_END
