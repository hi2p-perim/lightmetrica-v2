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
#include <lightmetrica/primitive.h>
#include <lightmetrica/trianglemesh.h>

LM_TEST_NAMESPACE_BEGIN

struct AccelTest : public ::testing::TestWithParam<const char*>
{
    virtual auto SetUp() -> void override { Logger::SetVerboseLevel(2); Logger::Run(); }
    virtual auto TearDown() -> void override { Logger::Stop(); }
};

INSTANTIATE_TEST_CASE_P(AccelTypes, AccelTest, ::testing::Values("naiveaccel"));

// --------------------------------------------------------------------------------

#pragma region Stub triangle mesh

// {(x, y, z) : 0<=x,y<=1, z=0,-1}
class StubTriangleMesh : public TriangleMesh
{
public:

    LM_IMPL_CLASS(StubTriangleMesh, TriangleMesh);

public:

    LM_IMPL_F(NumVertices) = [this]() -> int { return (int)(ps.size()); };
    LM_IMPL_F(NumFaces)    = [this]() -> int { return (int)(fs.size()); };
    LM_IMPL_F(Positions)   = [this]() -> const Float* { return ps.empty() ? nullptr : ps.data(); };
    LM_IMPL_F(Normals)     = [this]() -> const Float* { return ns.empty() ? nullptr : ns.data(); };
    LM_IMPL_F(Texcoords)   = [this]() -> const Float* { return ts.empty() ? nullptr : ts.data(); };
    LM_IMPL_F(Faces)       = [this]() -> const unsigned int* { return fs.empty() ? nullptr : fs.data(); };

protected:

    std::vector<Float> ps{
        0, 0, 0,
        1, 0, 0,
        1, 1, 0,
        0, 1, 0,
        0, 0, -1,
        1, 0, -1,
        1, 1, -1,
        0, 1, -1
    };
    std::vector<Float> ns{
        0, 0, 1,
        0, 0, 1,
        0, 0, 1,
        0, 0, 1,
        0, 0, 1,
        0, 0, 1,
        0, 0, 1,
        0, 0, 1
    };
    std::vector<Float> ts{
        0, 0,
        1, 0,
        1, 1,
        0, 1,
        0, 0,
        1, 0,
        1, 1,
        0, 1
    };
    std::vector<unsigned int> fs{
        0, 1, 2,
        0, 2, 3,
        4, 5, 6,
        4, 6, 7
    };

};

LM_COMPONENT_REGISTER_IMPL(StubTriangleMesh, "test_accel:stub_trianglemesh");

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Stub scene

class Stub_Scene : public Scene
{
public:

    LM_IMPL_CLASS(Stub_Scene, Scene);

public:

    LM_IMPL_F(NumPrimitives) = [this]() -> int { return 1; };
    LM_IMPL_F(PrimitiveAt) = [this](int index) -> const Primitive* { return &primitive_; };

public:

    Stub_Scene()
    {
        primitive_.transform = Mat4::Identity();
        primitive_.mesh = ;
    }

private:

    Primitive primitive_;

};

LM_COMPONENT_REGISTER_IMPL(Stub_Scene, "test_accel:stub_scene");

#pragma endregion

// --------------------------------------------------------------------------------

TEST_P(AccelTest, Simple)
{
    const auto scene = ComponentFactory::Create<Scene>("test_accel:stub_scene");
    const auto accel = ComponentFactory::Create<Accel>(GetParam());
    EXPECT_TRUE(accel->Initialize(nullptr));
    EXPECT_TRUE(accel->Build(*scene.get()));

    
}

LM_TEST_NAMESPACE_END
