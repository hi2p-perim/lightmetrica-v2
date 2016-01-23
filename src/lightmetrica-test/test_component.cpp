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

#pragma region Basic tests (simple instance creation, inherited interface)

struct A : public Component
{
    LM_INTERFACE_CLASS(A, Component);
    LM_INTERFACE_F(Func1, void(int));
    LM_INTERFACE_F(Func2, int(int, int));
    LM_INTERFACE_F(Func3, void());
    LM_INTERFACE_CLASS_END(A);
};

struct B : public A
{
    LM_INTERFACE_CLASS(B, A);
    LM_INTERFACE_F(Func4, void());
    LM_INTERFACE_CLASS_END(B);
};

struct A1 final : public A
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

LM_COMPONENT_REGISTER_IMPL_DEFAULT(A1);

struct B1 final : public B
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

LM_COMPONENT_REGISTER_IMPL_DEFAULT(B1);

TEST(ComponentTest, Simple)
{
    auto p = ComponentFactory::Create<A>("A1");
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
    auto p = ComponentFactory::Create<B>("B1");

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

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Tests with portable arguments

struct C : public Component
{
    LM_INTERFACE_CLASS(C, Component);
    LM_INTERFACE_F(Func1, void(const int*, int n));
    LM_INTERFACE_F(Func2, void(std::vector<int>));
    LM_INTERFACE_F(Func3, void(int&));
    LM_INTERFACE_F(Func4, void(const int&));
    LM_INTERFACE_F(Func5, void(const std::string&));
    LM_INTERFACE_CLASS_END(C);
};

struct C1 final : public C
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

LM_COMPONENT_REGISTER_IMPL_DEFAULT(C1);

TEST(ComponentTest, PortableArguments)
{
    auto p = ComponentFactory::Create<C>("C1");

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

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Tests with internal functions

// Some member exposes public interfaces and on the inherited class,
// the internal member is defined as virtual function
// this is not accessed from public interfaces (see `InternalInterface` test).
// Another way to introduce internal members is to use virtual inheritance
// along with multiple inheritance. However, this is possible because 
// we need to know the declaration of implementation class (see `InternalInterfaceMultiple` test).

struct D : public Component
{
    LM_INTERFACE_CLASS(D, Component);
    LM_INTERFACE_F(Func_Public, void());
    LM_INTERFACE_CLASS_END(D);
};

struct D_Internal
{
    virtual auto Func_Internal() const -> void = 0;
};

struct D_ final : public D_Internal, public D
{
    LM_IMPL_CLASS(D_, D);

    LM_IMPL_F(Func_Public) = [this]() -> void
    {
        std::cout << "hello" << std::endl;
    };

    virtual auto Func_Internal() const -> void override
    {
        std::cout << "world" << std::endl;
    }
};

LM_COMPONENT_REGISTER_IMPL_DEFAULT(D_);

TEST(ComponentTest, InternalInterfaceMultiple)
{
    const auto p = ComponentFactory::Create<D>();

    ASSERT_FALSE(p == nullptr);

    EXPECT_EQ("hello\n", TestUtils::CaptureStdout([&]()
    {
        p->Func_Public();
    }));

    EXPECT_EQ("world\n", TestUtils::CaptureStdout([&]()
    {
        // need to know D_
        D_Internal* p2 = static_cast<D_*>(p.get());
        p2->Func_Internal();
    }));
}

struct E : public Component
{
    LM_INTERFACE_CLASS(E, Component);
    LM_INTERFACE_F(Func_Public, void());
    LM_INTERFACE_CLASS_END(E);
};

struct E_Internal : public E
{
    virtual auto Func_Internal() const -> void = 0;
};

struct E_ final : public E_Internal
{
    LM_IMPL_CLASS(E_, E_Internal);

    LM_IMPL_F(Func_Public) = [this]() -> void
    {
        std::cout << "hello" << std::endl;
    };

    virtual auto Func_Internal() const -> void override
    {
        std::cout << "world" << std::endl;
    }
};

LM_COMPONENT_REGISTER_IMPL_DEFAULT(E_);

TEST(ComponentTest, InternalInterface)
{
    const auto p = ComponentFactory::Create<E>();

    ASSERT_FALSE(p == nullptr);

    EXPECT_EQ("hello\n", TestUtils::CaptureStdout([&]()
    {
        p->Func_Public();
    }));

    EXPECT_EQ("world\n", TestUtils::CaptureStdout([&]()
    {
        auto* p2 = static_cast<E_Internal*>(p.get());
        p2->Func_Internal();
    }));
}

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Tests with portable member variables

struct F : public Component
{
    Portable<std::string> id;
    LM_INTERFACE_CLASS(F, Component);
    LM_INTERFACE_F(Func, int());
    auto ID() const -> std::string { return id.Get(); }
    LM_INTERFACE_CLASS_END(F);
};

struct F_ : public F
{
    LM_IMPL_CLASS(F_, F);
    LM_IMPL_F(Func) = [this]() -> int { return 42; };
};

LM_COMPONENT_REGISTER_IMPL_DEFAULT(F_);

TEST(ComponentTest, PortableMemberVariable)
{
    const auto p = ComponentFactory::Create<F>();
    ASSERT_NE(nullptr, p);
    p->id.Reset("hello");
    EXPECT_EQ(42, p->Func());
    EXPECT_EQ("hello", p->ID());
}

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Contructor and descturctor

struct G : public Component
{
    LM_INTERFACE_CLASS(G, Component);
    LM_INTERFACE_F(Func, void());
    LM_INTERFACE_CLASS_END(G);
};

struct G_ final : public G
{
    LM_IMPL_CLASS(G_, G);

    G_()
    {
        std::cout << "ctor" << std::endl;
    }

    ~G_()
    {
        std::cout << "dtor" << std::endl;
    }

    LM_IMPL_F(Func) = [this]() -> void
    {
        std::cout << "hello" << std::endl;
    };
};

LM_COMPONENT_REGISTER_IMPL_DEFAULT(G_);

TEST(ComponentTest, ContructorAndDescturctor)
{
    EXPECT_EQ("ctor\nhello\ndtor\n", TestUtils::CaptureStdout([&]()
    {
        const auto p = ComponentFactory::Create<G>();
        p->Func();
    }));
}

#pragma endregion

LM_TEST_NAMESPACE_END
