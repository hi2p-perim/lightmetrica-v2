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
#include <lightmetrica/random.h>

LM_TEST_NAMESPACE_BEGIN

TEST(RandomTest, GetAndSetState)
{
    Random rng;
    rng.SetSeed(1);
    
    // Idle
    LM_UNUSED(rng.Next());
    LM_UNUSED(rng.Next());
    LM_UNUSED(rng.Next());
    LM_UNUSED(rng.Next());
    LM_UNUSED(rng.Next());

    // Get state
    const auto state = rng.GetInternalState();
    
    // Idle
    std::vector<Float> vs;
    vs.push_back(rng.Next());
    vs.push_back(rng.Next());
    vs.push_back(rng.Next());
    vs.push_back(rng.Next());
    vs.push_back(rng.Next());

    // Restore state
    rng.SetInternalState(state);
    EXPECT_TRUE(ExpectNear(vs[0], rng.Next()));
    EXPECT_TRUE(ExpectNear(vs[1], rng.Next()));
    EXPECT_TRUE(ExpectNear(vs[2], rng.Next()));
    EXPECT_TRUE(ExpectNear(vs[3], rng.Next()));
    EXPECT_TRUE(ExpectNear(vs[4], rng.Next()));
}

LM_TEST_NAMESPACE_END
