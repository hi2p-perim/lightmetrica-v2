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
#include <lightmetrica/metacounter.h>
#include <lightmetrica-test/utils.h>

LM_TEST_NAMESPACE_BEGIN

TEST(MetaCounterTest, Simple)
{
    using C1 = MetaCounter<class Counter1>;
    using C2 = MetaCounter<class Counter2>;

    C1::Next();
    C1::Next();
    C1::Next();

    C2::Next();
    C2::Next();

    static_assert(C1::Value() == 3, "C1::Value () == 3");
    static_assert(C2::Value() == 2, "C2::Value () == 2");

    EXPECT_EQ(3, C1::Value());
    EXPECT_EQ(2, C2::Value());
}

LM_TEST_NAMESPACE_END
