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
#include <lightmetrica/scene.h>
#include <lightmetrica/property.h>
#include <lightmetrica/primitive.h>
#include <lightmetrica-test/utils.h>

LM_TEST_NAMESPACE_BEGIN

struct SceneTest : public ::testing::Test
{
    virtual auto SetUp() -> void override { Logger::Run(); }
    virtual auto TearDown() -> void override { Logger::Stop(); }
};

// Tests simple loading of the scene
TEST_F(SceneTest, SimpleLoad)
{
    const auto SimpleLoad_Input = TestUtils::MultiLineLiteral(R"x(
    | lightmetrica_scene:
    |
    |   version: 1.0.0
    |
    |   assets:
    |     - id: stub_asset_1
    |       asset_type: stub
    |       type: stub_1
    |       params:
    |           A: a
    |           B: b
    |
    |   accel:
    |     type: stub_accel
    |     params:
    |       A: a
    |       B: b
    |
    |   scene:
    |     main_camera: n1
    |     nodes:
    |       - id: n1
    |       - id: n2
    |         child:
    |           - id: n2_1
    |           - id: n2_2
    |             child:
    |               - id: n2_2_1
    |               - id: n2_2_2
    )x");

    const auto prop = ComponentFactory::Create<PropertyTree>();
    EXPECT_TRUE(prop->LoadFromString(SimpleLoad_Input));
    
    const auto scene = ComponentFactory::Create<Scene>();
    EXPECT_TRUE(scene->Initialize(prop->Root()));

    EXPECT_EQ("n1", scene->PrimitiveByID("n1")->id);
    EXPECT_EQ("n2", scene->PrimitiveByID("n2")->id);
    EXPECT_EQ("n2_1", scene->PrimitiveByID("n2_1")->id);
    EXPECT_EQ("n2_2", scene->PrimitiveByID("n2_2")->id);
    EXPECT_EQ("n2_2_1", scene->PrimitiveByID("n2_2_1")->id);
    EXPECT_EQ("n2_2_2", scene->PrimitiveByID("n2_2_2")->id);
}

// Tests with the scene with transform
TEST_F(SceneTest, Transform)
{
    const auto Transform_Input = TestUtils::MultiLineLiteral(R"x(
    | lightmetrica_scene:
    |   version: 1.0.0
    |   scene:
    |     main_camera: n1
    |     nodes:
    |       - id: n1
    |         transform:
    |           # Transform specified by a 4x4 matrix (row major)
    |           matrix: >
    |             1 0 0 0
    |             0 1 0 0
    |             0 0 1 0
    |             0 0 0 1
    |
    |       - id: n2
    |         transform:
    |           # Transform by translate, rotate, and scale
    |           translate: 0 0 0
    |           scale: 1
    |           rotate:
    |             # Specify rotation by rotation axis and angle
    |             axis: 0 1 0
    |             angle: 45
    |              
    |       # Accumulated transform by multiple levels of nodes
    |       - id: n3
    |         transform:
    |           matrix: >
    |             1 0 0 1
    |             0 1 0 1
    |             0 0 1 1
    |             0 0 0 1
    |         child:
    |           - id: n4_1
    |             transform:
    |               matrix: >
    |                 2 0 0 0
    |                 0 2 0 0
    |                 0 0 2 0
    |                 0 0 0 1
    )x");

    const auto prop = ComponentFactory::Create<PropertyTree>();
    EXPECT_TRUE(prop->LoadFromString(Transform_Input));

    const auto scene = ComponentFactory::Create<Scene>();
    EXPECT_TRUE(scene->Initialize(prop->Root()));



}

// Camera nodes
TEST_F(SceneTest, CameraNode)
{
    const auto CameraNode_Input = TestUtils::MultiLineLiteral(R"x(
    | lightmetrica_scene:
    |   version: 1.0.0
    |   scene:
    |     main_camera: {{main_camera_node}}
    |     - id: n1
    )x");

    FAIL();
}

// Missing `lightmetrica_scene` node
TEST_F(SceneTest, InvalidrRootNode_Fail)
{
    const auto InvalidrRootNode_Fail_Input = TestUtils::MultiLineLiteral(R"x(
    | a:
    )x");

    const auto prop = ComponentFactory::Create<PropertyTree>();
    EXPECT_TRUE(prop->LoadFromString(InvalidrRootNode_Fail_Input));

    const auto scene = ComponentFactory::Create<Scene>();
    const auto err = TestUtils::ExtractLogMessage(TestUtils::CaptureStdout([&]()
    {
        EXPECT_FALSE(scene->Initialize(prop->Root()));
        Logger::Flush();
    }));
    EXPECT_EQ("Missing 'lightmetrica_scene' node", err);
}

// Missing `version` node
TEST_F(SceneTest, MissingVersionNode_Fail)
{
    const auto MissingVersionNode_Fail_Input = TestUtils::MultiLineLiteral(R"x(
    | lightmetrica_scene:
    |   a:
    )x");

    const auto prop = ComponentFactory::Create<PropertyTree>();
    EXPECT_TRUE(prop->LoadFromString(MissingVersionNode_Fail_Input));

    const auto scene = ComponentFactory::Create<Scene>();
    const auto err = TestUtils::ExtractLogMessage(TestUtils::CaptureStdout([&]()
    {
        EXPECT_FALSE(scene->Initialize(prop->Root()));
        Logger::Flush();
    }));
    EXPECT_EQ("Missing 'version' node", err);
}

// Invalid version string
TEST_F(SceneTest, InvalidVersionString_Fail)
{
    const auto InvalidVersionString_Fail_Input = TestUtils::MultiLineLiteral(R"x(
    | lightmetrica_scene:
    |   version: 1.0
    )x");

    const auto prop = ComponentFactory::Create<PropertyTree>();
    EXPECT_TRUE(prop->LoadFromString(InvalidVersionString_Fail_Input));

    const auto scene = ComponentFactory::Create<Scene>();
    const auto err = TestUtils::ExtractLogMessage(TestUtils::CaptureStdout([&]()
    {
        EXPECT_FALSE(scene->Initialize(prop->Root()));
        Logger::Flush();
    }));
    EXPECT_TRUE(boost::starts_with(err, "Invalid version string"));
}

// Version check fails
TEST_F(SceneTest, InvalidVersion_Fail)
{
    const auto InvalidVersion_Fail_Input = TestUtils::MultiLineLiteral(R"x(
    | lightmetrica_scene:
    |   version: 0.0.0
    )x");

    const auto prop = ComponentFactory::Create<PropertyTree>();
    EXPECT_TRUE(prop->LoadFromString(InvalidVersion_Fail_Input));

    const auto scene = ComponentFactory::Create<Scene>();
    const auto err = TestUtils::ExtractLogMessage(TestUtils::CaptureStdout([&]()
    {
        EXPECT_FALSE(scene->Initialize(prop->Root()));
        Logger::Flush();
    }));
    EXPECT_TRUE(boost::starts_with(err, "Invalid version"));
}

// There is no `main_camera` node
TEST_F(SceneTest, NoMainCamera_Fail)
{
    const auto NoMainCamera_Fail_Input = TestUtils::MultiLineLiteral(R"x(
    | lightmetrica_scene:
    |   version: 1.0.0
    |   scene:
    |     - id: n1
    |     - id: n2
    )x");

    FAIL();
}

// Invalid number of arguments in `transform`
TEST_F(SceneTest, Transform_Fail)
{
    const auto Transform_Fail_Input = TestUtils::MultiLineLiteral(R"x(
    | lightmetrica_scene:
    |   version: 1.0.0
    |   scene:
    |     main_camera: n1
    |     nodes:
    |       - id: n1
    |         transform: {{transform}}
    )x");

    const std::string TransformNodes[] =
    {
        TestUtils::MultiLineLiteral(R"x(
        | {}
        )x")
    };

    FAIL();
}

LM_TEST_NAMESPACE_END
