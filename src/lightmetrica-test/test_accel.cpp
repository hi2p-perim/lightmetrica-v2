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
#include <lightmetrica/accel.h>
#include <lightmetrica/scene.h>

LM_TEST_NAMESPACE_BEGIN

struct AccelTest : public ::testing::TestWithParam<const char*>
{
    virtual auto SetUp() -> void override { Logger::Run(); }
    virtual auto TearDown() -> void override { Logger::Stop(); }
};

INSTANTIATE_TEST_CASE_P(AccelTypes, AccelTest, ::testing::Values("naiveaccel"));

// --------------------------------------------------------------------------------

struct Stub_Scene : public Scene
{
    LM_IMPL_CLASS(Stub_Scene, Scene);

};

LM_COMPONENT_REGISTER_IMPL(Stub_Scene, "test_accel:stub_scene");

// --------------------------------------------------------------------------------

TEST_P(AccelTest, Simple)
{
    const auto scene = ComponentFactory::Create<Scene>("test_accel:stub_scene");
    const auto accel = ComponentFactory::Create<Accel>(GetParam());
    EXPECT_TRUE(accel->Initialize(nullptr));
    EXPECT_TRUE(accel->Build(*scene.get()));

    
}

LM_TEST_NAMESPACE_END
