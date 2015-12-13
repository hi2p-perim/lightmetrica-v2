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
#include <lightmetrica/component.h>
#include <lightmetrica-test/utils.h>

LM_TEST_NAMESPACE_BEGIN

// --------------------------------------------------------------------------------

#pragma region Interfaces

struct A : public Component
{
    LM_INTERFACE_CLASS(A, Component, 3);
    LM_INTERFACE_F(0, Func1, void(int));
    LM_INTERFACE_F(1, Func2, int(int, int));
    LM_INTERFACE_F(2, Func3, void());
};

struct B : public A
{
    LM_INTERFACE_CLASS(B, A, 1);
    LM_INTERFACE_F(0, Func4, void());
};

struct C : public Component
{
    LM_INTERFACE_CLASS(C, Component, 6);
    LM_INTERFACE_F(0, Func1, void(const int*, int n));
    LM_INTERFACE_F(1, Func2, void(std::vector<int>));
    LM_INTERFACE_F(2, Func3, void(int&));
    LM_INTERFACE_F(3, Func4, void(const int&));
    LM_INTERFACE_F(4, Func5, void(const std::string&));
};

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Implementation

struct A1 : public A
{
    LM_IMPL_CLASS(A1, A);

    LM_IMPL_F(Func1) = [this](int v) -> void
    {
        std::cout << v << std::endl;
    };

    LM_IMPL_F(Func2) = [this](int v1, int v2) -> int
    {
        return v1 + v2;
    };

    LM_IMPL_F(Func3) = [this]() -> void
    {
        std::cout << "hello" << std::endl;
    };
};

LM_COMPONENT_REGISTER_IMPL(A1);

// --------------------------------------------------------------------------------

struct B1 : public B
{
    LM_IMPL_CLASS(B1, B);

    LM_IMPL_F(Func1) = [this](int v) -> void
    {
        std::cout << v + 1 << std::endl;
    };

    LM_IMPL_F(Func2) = [this](int v1, int v2) -> int
    {
        return v1 + v2 + 1;
    };

    LM_IMPL_F(Func3) = [this]() -> void
    {
        std::cout << "a" << std::endl;
    };

    LM_IMPL_F(Func4) = [this]() -> void
    {
        std::cout << "b" << std::endl;
    };
};

LM_COMPONENT_REGISTER_IMPL(B1);

// --------------------------------------------------------------------------------

struct C1 : public C
{
    LM_IMPL_CLASS(C1, C);

    LM_IMPL_F(Func1) = [this](const int* v, int n) -> void
    {
        for (int i = 0; i < n; i++) std::cout << v[i] << " ";
        std::cout << std::endl;
    };

    LM_IMPL_F(Func2) = [this](std::vector<int> v) -> void
    {
        for (int& val : v) std::cout << val << " ";
        std::cout << std::endl;
    };

    LM_IMPL_F(Func3) = [this](int& v) -> void
    {
        v = 42;
    };

    LM_IMPL_F(Func4) = [this](const int& v) -> void
    {
        std::cout << v << std::endl;
    };

    LM_IMPL_F(Func5) = [this](const std::string& s) -> void
    {
        std::cout << s << std::endl;
    };
};

LM_COMPONENT_REGISTER_IMPL(C1);

#pragma endregion

// --------------------------------------------------------------------------------

TEST(ComponentTest, Simple)
{
    auto p = std::move(ComponentFactory::Create<A>("A1"));
    ASSERT_FALSE(p == nullptr);

    EXPECT_EQ("42\n", TestUtils::CaptureStdout([&]()
    {
        p->Func1(42);
    }));

    EXPECT_EQ(3, p->Func2(1, 2));

    EXPECT_EQ("hello\n", TestUtils::CaptureStdout([&]()
    {
        p->Func3();
    }));
}

TEST(ComponentTest, FailedToCreate)
{
    auto* p = ComponentFactory::Create("A2");
    ASSERT_TRUE(p == nullptr);
}

TEST(ComponentTest, InheritedInterface)
{
    auto p = std::move(ComponentFactory::Create<B>("B1"));

    ASSERT_FALSE(p == nullptr);

    EXPECT_EQ("43\n", TestUtils::CaptureStdout([&]()
    {
        p->Func1(42);
    }));

    EXPECT_EQ(4, p->Func2(1, 2));

    EXPECT_EQ("a\n", TestUtils::CaptureStdout([&]()
    {
        p->Func3();
    }));

    EXPECT_EQ("b\n", TestUtils::CaptureStdout([&]()
    {
        p->Func4();
    }));
}

TEST(ComponentTest, PortableArguments)
{
    auto p = std::move(ComponentFactory::Create<C>("C1"));

    std::vector<int> v{ 1,2,3 };
    
    EXPECT_EQ("1 2 3 \n", TestUtils::CaptureStdout([&]()
    {
        p->Func1(v.data(), 3);
    }));

    EXPECT_EQ("1 2 3 \n", TestUtils::CaptureStdout([&]()
    {
        p->Func2(v);
    }));

    {
        int t;
        p->Func3(t);
        EXPECT_EQ(42, t);
    }

    EXPECT_EQ("42\n", TestUtils::CaptureStdout([&]()
    {
        int t = 42;
        p->Func4(t);
    }));

    EXPECT_EQ("hello\n", TestUtils::CaptureStdout([&]()
    {
        std::string str = "hello";
        p->Func5(str);
    }));
}

LM_TEST_NAMESPACE_END
