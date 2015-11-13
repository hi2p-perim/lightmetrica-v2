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

#include <lightmetrica/component.h>
#include <vector>
#include <string>

LM_NAMESPACE_BEGIN

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

LM_NAMESPACE_END
