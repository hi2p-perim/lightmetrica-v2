/*
    Lightmetrica - A modern, research-oriented renderer
    
    Copyright (c) 2015 Hisanari Otsu
    
    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files(the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions :
    
    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.
    
    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.
*/

#include <pch.h>
#include <lightmetrica/test.h>

LM_NAMESPACE_BEGIN

struct A1 : public A
{
    LM_IMPL_CLASS(A1, A);

    LM_IMPL_F(Func1, [this](int v)
    {
        std::cout << v << std::endl;
    }); 

    LM_IMPL_F(Func2, [this](int v1, int v2)
    {
        return v1 + v2;
    });

    LM_IMPL_F(Func3, [this]()
    {
        std::cout << "hello" << std::endl;
    });
};

LM_COMPONENT_REGISTER_IMPL(A1);

// --------------------------------------------------------------------------------

struct B1 : public B
{
    LM_IMPL_CLASS(B1, B);

    LM_IMPL_F(Func1, [this](int v)
    {
        std::cout << v + 1 << std::endl;
    });

    LM_IMPL_F(Func2, [this](int v1, int v2)
    {
        return v1 + v2 + 1;
    });

    LM_IMPL_F(Func3, [this]()
    {
        std::cout << "a" << std::endl;
    });

    LM_IMPL_F(Func4, [this]()
    {
        std::cout << "b" << std::endl;
    });
};

LM_COMPONENT_REGISTER_IMPL(B1);

// --------------------------------------------------------------------------------

struct C1 : public C
{
    LM_IMPL_CLASS(C1, C);

    LM_IMPL_F(Func1, [this](const int* v, int n)
    {
        for (int i = 0; i < n; i++) std::cout << v[i] << " ";
        std::cout << std::endl;
    });

    LM_IMPL_F(Func2, [this](std::vector<int> v)
    {
        for (int& val : v) std::cout << val << " ";
        std::cout << std::endl;
    });

    LM_IMPL_F(Func3, [this](int& v)
    {
        v = 42;
    });

    LM_IMPL_F(Func4, [this](const int& v)
    {
        std::cout << v << std::endl;
    });

    LM_IMPL_F(Func5, [this](const std::string& s)
    {
        std::cout << s << std::endl;
    });
};

LM_COMPONENT_REGISTER_IMPL(C1);

LM_NAMESPACE_END