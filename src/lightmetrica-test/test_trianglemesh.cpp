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
#include <lightmetrica/trianglemesh.h>
#include <lightmetrica/property.h>
#include <lightmetrica-test/utils.h>
#include <lightmetrica-test/mathutils.h>

LM_TEST_NAMESPACE_BEGIN

struct TriangleMeshTest : public ::testing::Test
{
    virtual auto SetUp() -> void override { Logger::SetVerboseLevel(2); Logger::Run(); }
    virtual auto TearDown() -> void override { Logger::Stop(); }
};

TEST_F(TriangleMeshTest, Raw)
{
    const auto prop = ComponentFactory::Create<PropertyTree>();
    ASSERT_TRUE(prop->LoadFromString(TestUtils::MultiLineLiteral(R"x(
    | positions: >
    |   0 0 0
    |   1 0 0
    |   1 1 0
    |   0 1 0
    | normals: >
    |   0 0 1
    |   0 0 1
    |   0 0 1
    |   0 0 1
    | texcoords: >
    |   0 0
    |   1 0
    |   1 1
    |   0 1
    | faces: >
    |   0 1 2
    |   0 2 3
    )x")));

    const auto mesh = ComponentFactory::Create<TriangleMesh>("trianglemesh::raw");
    ASSERT_NE(nullptr, mesh);
    ASSERT_TRUE(mesh->Load(prop->Root(), nullptr, nullptr));

    const Float ans_ps[] =
    {
        0, 0, 0,
        1, 0, 0,
        1, 1, 0,
        0, 1, 0,
    };
    const Float ans_ns[] =
    {
        0, 0, 1,
        0, 0, 1,
        0, 0, 1,
        0, 0, 1,
    };
    const Float ans_ts[] =
    {
        0, 0,
        1, 0,
        1, 1,
        0, 1,
    };
    const unsigned int ans_fs[] =
    {
        0, 1, 2,
        0, 2, 3,
    };

    ASSERT_EQ(4, mesh->NumVertices());
    ASSERT_EQ(2, mesh->NumFaces());

    const auto* ps = mesh->Positions();
    const auto* ns = mesh->Normals();
    const auto* ts = mesh->Texcoords();
    ASSERT_NE(nullptr, ps);
    ASSERT_NE(nullptr, ns);
    ASSERT_NE(nullptr, ts);
    for (int i = 0; i < 4; i++)
    {
        EXPECT_TRUE(ExpectNear(ans_ps[3*i+0], ps[3*i+0]));
        EXPECT_TRUE(ExpectNear(ans_ps[3*i+1], ps[3*i+1]));
        EXPECT_TRUE(ExpectNear(ans_ps[3*i+2], ps[3*i+2]));
        EXPECT_TRUE(ExpectNear(ans_ns[3*i+0], ns[3*i+0]));
        EXPECT_TRUE(ExpectNear(ans_ns[3*i+1], ns[3*i+1]));
        EXPECT_TRUE(ExpectNear(ans_ns[3*i+2], ns[3*i+2]));
        EXPECT_TRUE(ExpectNear(ans_ts[2*i+0], ts[2*i+0]));
        EXPECT_TRUE(ExpectNear(ans_ts[2*i+1], ts[2*i+1]));
    }

    const auto* fs = mesh->Faces();
    ASSERT_NE(nullptr, fs);
    for (int i = 0; i < 2; i++)
    {
        EXPECT_EQ(ans_fs[3*i+0], fs[3*i+0]);
        EXPECT_EQ(ans_fs[3*i+1], fs[3*i+1]);
        EXPECT_EQ(ans_fs[3*i+2], fs[3*i+2]);
    }
}

LM_TEST_NAMESPACE_END
