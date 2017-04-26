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
#include <lightmetrica/scene3.h>
#include <lightmetrica/property.h>
#include <lightmetrica/primitive.h>
#include <lightmetrica-test/utils.h>
#include <lightmetrica/assets.h>
#include <lightmetrica/accel3.h>
#include <lightmetrica/light.h>
#include <lightmetrica/sensor.h>
#include <lightmetrica/bsdf.h>
#include <lightmetrica/trianglemesh.h>
#include <lightmetrica/detail/serial.h>
#include <lightmetrica-test/mathutils.h>

LM_TEST_NAMESPACE_BEGIN

struct Scene3Test : public ::testing::Test
{
    virtual auto SetUp() -> void override { Logger::SetVerboseLevel(2); Logger::Run(); }
    virtual auto TearDown() -> void override { Logger::Stop(); }
};

// --------------------------------------------------------------------------------

struct Stub_Assets : public Assets
{
    LM_IMPL_CLASS(Stub_Assets, Assets);
    LM_IMPL_F(AssetByIDAndType) = [this](const std::string& id, const std::string& type, const Primitive* primitive) -> Asset* { return nullptr; };
    LM_IMPL_F(PostLoad) = [this](const Scene* scene) -> bool { return true; };
};

struct Stub_Accel : public Accel
{
    LM_IMPL_CLASS(Stub_Accel, Accel);
    LM_IMPL_F(Build) = [this](const Scene* scene) -> bool
    {
        return true;
    };
};

LM_COMPONENT_REGISTER_IMPL_DEFAULT(Stub_Assets);
LM_COMPONENT_REGISTER_IMPL_DEFAULT(Stub_Accel);

// --------------------------------------------------------------------------------

// Tests simple loading of the scene
TEST_F(Scene3Test, SimpleLoad)
{
    const auto SimpleLoad_Input = TestUtils::MultiLineLiteral(R"x(
    | sensor: n1
    | nodes:
    |   - id: n1
    |   - id: n2
    |     child:
    |       - id: n2_1
    |       - id: n2_2
    |         child:
    |           - id: n2_2_1
    |           - id: n2_2_2
    )x");

    const auto prop = ComponentFactory::Create<PropertyTree>();
    ASSERT_TRUE(prop->LoadFromString(SimpleLoad_Input));

    const auto assets = ComponentFactory::Create<Assets>("Stub_Assets");
    const auto accel = ComponentFactory::Create<Accel3>("Stub_Accel");
    const auto scene = ComponentFactory::Create<Scene3>("scene::scene3");
    ASSERT_TRUE(scene->Initialize(prop->Root(), assets.get(), accel.get()));

    EXPECT_EQ("n1", std::string(scene->PrimitiveByID("n1")->id));
    EXPECT_EQ("n2", std::string(scene->PrimitiveByID("n2")->id));
    EXPECT_EQ("n2_1",   std::string(scene->PrimitiveByID("n2_1")->id));
    EXPECT_EQ("n2_2",   std::string(scene->PrimitiveByID("n2_2")->id));
    EXPECT_EQ("n2_2_1", std::string(scene->PrimitiveByID("n2_2_1")->id));
    EXPECT_EQ("n2_2_2", std::string(scene->PrimitiveByID("n2_2_2")->id));
}

// --------------------------------------------------------------------------------

// Stubs
struct Stub_Sensor : public Sensor
{
    LM_IMPL_CLASS(Stub_Sensor, Sensor);
    LM_IMPL_F(Load) = [this](const PropertyNode* prop, Assets* assets, const Primitive* primitive) -> bool { return true; };
};

struct Stub_Light : public Light
{
    LM_IMPL_CLASS(Stub_Light, Light);
    LM_IMPL_F(Load) = [this](const PropertyNode* prop, Assets* assets, const Primitive* primitive) -> bool { return true; };
};

struct Stub_BSDF : public BSDF
{
    LM_IMPL_CLASS(Stub_BSDF, BSDF);
    LM_IMPL_F(Load) = [this](const PropertyNode* prop, Assets* assets, const Primitive* primitive) -> bool { return true; };
};

struct Stub_TriangleMesh_1 : public TriangleMesh
{
    LM_IMPL_CLASS(Stub_TriangleMesh_1, TriangleMesh);
    LM_IMPL_F(Load) = [this](const PropertyNode* prop, Assets* assets, const Primitive* primitive) -> bool { return true; };
    LM_IMPL_F(NumVertices) = [this]() -> int { return 0; };
    LM_IMPL_F(Positions) = [this]() -> const Float*{ return nullptr; };
};

struct Stub_TriangleMesh_2 : public TriangleMesh
{
    LM_IMPL_CLASS(Stub_TriangleMesh_2, TriangleMesh);
    LM_IMPL_F(Load) = [this](const PropertyNode* prop, Assets* assets, const Primitive* primitive) -> bool { return true; };
    LM_IMPL_F(NumVertices) = [this]() -> int { return 0; };
    LM_IMPL_F(Positions) = [this]() -> const Float*{ return nullptr; };
};

LM_COMPONENT_REGISTER_IMPL(Stub_Sensor, "sensor::stub_sensor");
LM_COMPONENT_REGISTER_IMPL(Stub_Light, "light::stub_light");
LM_COMPONENT_REGISTER_IMPL(Stub_TriangleMesh_1, "trianglemesh::stub_trianglemesh_1");
LM_COMPONENT_REGISTER_IMPL(Stub_TriangleMesh_2, "trianglemesh::stub_trianglemesh_2");
LM_COMPONENT_REGISTER_IMPL(Stub_BSDF, "bsdf::stub_bsdf");

// Tests simple loading of the scene with delayed loading of assets
TEST_F(Scene3Test, SimpleLoadWithAssets)
{
    const auto SimpleLoad_Input = TestUtils::MultiLineLiteral(R"x(
    | assets:
    |   sensor_1:
    |     interface: sensor
    |     type: stub_sensor
    |
    |   light_1:
    |     interface: light
    |     type: stub_light
    |
    |   mesh_1:
    |     interface: trianglemesh
    |     type: stub_trianglemesh_1
    |
    |   mesh_2:
    |     interface: trianglemesh
    |     type: stub_trianglemesh_2
    |
    |   bsdf_1:
    |     interface: bsdf
    |     type: stub_bsdf
    |
    | scene:
    |   sensor: n1
    |
    |   accel:
    |     type: stub_accel
    |
    |   nodes:
    |     - id: n1
    |       sensor: sensor_1
    |       mesh: mesh_1
    |       bsdf: bsdf_1
    |
    |     - id: n2
    |       light: light_1
    |       mesh: mesh_2
    |       bsdf: bsdf_1
    )x");

    const auto prop = ComponentFactory::Create<PropertyTree>();
    ASSERT_TRUE(prop->LoadFromString(SimpleLoad_Input));

    const auto assets = ComponentFactory::Create<Assets>("assets::assets3");
    EXPECT_TRUE(assets->Initialize(prop->Root()->Child("assets")));
    
    const auto accel = ComponentFactory::Create<Accel3>("Stub_Accel");
    const auto scene = ComponentFactory::Create<Scene3>("scene::scene3");
    ASSERT_TRUE(scene->Initialize(prop->Root()->Child("scene"), assets.get(), accel.get()));

    const auto* n1 = scene->PrimitiveByID("n1");
    ASSERT_NE(nullptr, n1->emitter);
    EXPECT_EQ("sensor_1", n1->emitter->ID());
    ASSERT_NE(nullptr, n1->bsdf);
    EXPECT_EQ("bsdf_1", n1->bsdf->ID());
    ASSERT_NE(nullptr, n1->mesh);
    EXPECT_EQ("mesh_1", n1->mesh->ID());
    
    const auto* n2 = scene->PrimitiveByID("n2");
    ASSERT_NE(nullptr, n2->emitter);
    EXPECT_EQ("light_1", n2->emitter->ID());
    ASSERT_NE(nullptr, n2->bsdf);
    EXPECT_EQ("bsdf_1", n2->bsdf->ID());
    ASSERT_NE(nullptr, n2->mesh);
    EXPECT_EQ("mesh_2", n2->mesh->ID());
}

// --------------------------------------------------------------------------------

// Tests with the scene with transform
TEST_F(Scene3Test, Transform)
{
    const auto Transform_Input = TestUtils::MultiLineLiteral(R"x(
    | sensor: n1
    | nodes:
    |   - id: n1
    |     transform:
    |       # Transform specified by a 4x4 matrix (row major)
    |       matrix: >
    |         1 0 0 0
    |         0 1 0 0
    |         0 0 1 0
    |         0 0 0 1
    |
    |   - id: n2
    |     transform:
    |       # Transform by translate, rotate, and scale
    |       translate: 0 0 0
    |       scale: 1 1 1
    |       rotate:
    |         # Specify rotation by rotation axis and angle
    |         axis: 0 1 0
    |         angle: 0
    |          
    |   # Accumulated transform by multiple levels of nodes
    |   - id: n3
    |     transform:
    |       matrix: >
    |         1 0 0 1
    |         0 1 0 1
    |         0 0 1 1
    |         0 0 0 1
    |     child:
    |       - id: n4
    |         transform:
    |           matrix: >
    |             2 0 0 0
    |             0 2 0 0
    |             0 0 2 0
    |             0 0 0 1
    )x");

    const auto prop = ComponentFactory::Create<PropertyTree>();
    EXPECT_TRUE(prop->LoadFromString(Transform_Input));

    const auto assets = ComponentFactory::Create<Assets>("Stub_Assets");
    const auto accel = ComponentFactory::Create<Accel3>("Stub_Accel");
    const auto scene = ComponentFactory::Create<Scene3>("scene::scene3");
    ASSERT_TRUE(scene->Initialize(prop->Root(), assets.get(), accel.get()));

    const Primitive* n1 = scene->PrimitiveByID("n1");
    EXPECT_TRUE(ExpectMatNear(Mat4{ 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 }, n1->transform));

    const Primitive* n2 = scene->PrimitiveByID("n2");
    EXPECT_TRUE(ExpectMatNear(Mat4{ 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 }, n2->transform));

    const Primitive* n3 = scene->PrimitiveByID("n3");
    EXPECT_TRUE(ExpectMatNear(Mat4{ 1,0,0,1, 0,1,0,1, 0,0,1,1, 0,0,0,1 }, n3->transform));

    const Primitive* n4 = scene->PrimitiveByID("n4");
    EXPECT_TRUE(ExpectMatNear(Mat4{ 2,0,0,2, 0,2,0,2, 0,0,2,2, 0,0,0,1 }, n4->transform));
}

// --------------------------------------------------------------------------------

// Sensor nodes
TEST_F(Scene3Test, SensorNode)
{
    const auto SensorNode_Input = TestUtils::MultiLineLiteral(R"x(
    | sensor: n2
    | nodes:
    |   - id: n1
    |   - id: n2
    )x");

    const auto prop = ComponentFactory::Create<PropertyTree>();
    EXPECT_TRUE(prop->LoadFromString(SensorNode_Input));

    const auto assets = ComponentFactory::Create<Assets>("Stub_Assets");
    const auto accel = ComponentFactory::Create<Accel3>("Stub_Accel");
    const auto scene = ComponentFactory::Create<Scene3>("scene::scene3");
    ASSERT_TRUE(scene->Initialize(prop->Root(), assets.get(), accel.get()));

    EXPECT_EQ("n2", std::string(scene->GetSensor()->id));
}

// --------------------------------------------------------------------------------

// Test for serialization (simplified case)
TEST_F(Scene3Test, SerializationSimple)
{
    // We do not provide serialization functions for stub assets and accel
    // Tests serialization for primitives

    const auto Input = TestUtils::MultiLineLiteral(R"x(
    | sensor: n1
    | nodes:
    |   - id: n1
    |   - id: n2
    )x");

    const auto prop = ComponentFactory::Create<PropertyTree>();
    EXPECT_TRUE(prop->LoadFromString(Input));

    const auto assets = ComponentFactory::Create<Assets>("Stub_Assets");
    const auto accel = ComponentFactory::Create<Accel3>("Stub_Accel");
    const auto scene = ComponentFactory::Create<Scene3>("scene::scene3");
    ASSERT_TRUE(scene->Initialize(prop->Root(), assets.get(), accel.get()));

    // Serialize the scene
    std::string serialized;
    {
        std::ostringstream ss(std::ios::binary);
        ASSERT_TRUE(scene->Serialize(ss));
        serialized = ss.str();
    }

    // Deserialize into a new scene instance
    auto scene2 = ComponentFactory::Create<Scene3>("scene::scene3");
    std::istringstream ss(serialized, std::ios::binary);
    ASSERT_TRUE(scene2->Deserialize(ss, { { "assets", assets.get() },{ "accel", accel.get() } }));

    // Check consistencies
    const auto* n1 = scene2->PrimitiveByID("n1");
    ASSERT_NE(nullptr, n1);
    EXPECT_EQ("n1", n1->id);
    const auto* n2 = scene2->PrimitiveByID("n2");
    ASSERT_NE(nullptr, n2);
    EXPECT_EQ("n2", n2->id);
}

struct Stub_BSDF_Serializable : public BSDF
{
    SPD R_;
    LM_IMPL_CLASS(Stub_BSDF_Serializable, BSDF);
    LM_IMPL_F(Load) = [this](const PropertyNode* prop, Assets* assets, const Primitive* primitive) -> bool
    {
        R_ = SPD(42);
        return true;
    };
    LM_IMPL_F(Reflectance) = [this]() -> SPD
    {
        return R_;
    };
    LM_IMPL_F(Serialize) = [this](std::ostream& stream) -> bool
    {
        cereal::PortableBinaryOutputArchive oa(stream);
        oa(R_);
        return true;
    };
    LM_IMPL_F(Deserialize) = [this](std::istream& stream, const std::unordered_map<std::string, void*>& userdata) -> bool
    {
        cereal::PortableBinaryInputArchive ia(stream);
        ia(R_);
        return true;
    };
};

struct Stub_TriangleMesh_Serializable : public TriangleMesh
{
    int nv_ = 1;
    std::vector<Float> ps_{ 1_f, 2_f, 3_f };
    LM_IMPL_CLASS(Stub_TriangleMesh_Serializable, TriangleMesh);
    LM_IMPL_F(Load) = [this](const PropertyNode* prop, Assets* assets, const Primitive* primitive) -> bool { return true; };
    LM_IMPL_F(NumVertices) = [this]() -> int { return nv_; };
    LM_IMPL_F(Positions) = [this]() -> const Float* { return ps_.data(); };
    LM_IMPL_F(Serialize) = [this](std::ostream& stream) -> bool
    {
        cereal::PortableBinaryOutputArchive oa(stream);
        oa(nv_, ps_);
        return true;
    };
    LM_IMPL_F(Deserialize) = [this](std::istream& stream, const std::unordered_map<std::string, void*>& userdata) -> bool
    {
        cereal::PortableBinaryInputArchive ia(stream);
        ia(nv_, ps_);
        return true;
    };
};

LM_COMPONENT_REGISTER_IMPL(Stub_BSDF_Serializable, "bsdf::stub_bsdf_serializable");
LM_COMPONENT_REGISTER_IMPL(Stub_TriangleMesh_Serializable, "trianglemesh::stub_trianglemesh_serializable");

// Test for serialization (with serializable assets)
TEST_F(Scene3Test, SerializationWithAssets)
{
    const auto Input = TestUtils::MultiLineLiteral(R"x(
    | assets:
    |   mesh_1:
    |     interface: trianglemesh
    |     type: stub_trianglemesh_serializable
    |
    |   bsdf_1:
    |     interface: bsdf
    |     type: stub_bsdf_serializable
    |
    | scene:
    |   sensor: n1
    |   nodes:
    |     - id: n1
    |     - id: n2
    |       mesh: mesh_1
    |       bsdf: bsdf_1
    )x");

    const auto prop = ComponentFactory::Create<PropertyTree>();
    ASSERT_TRUE(prop->LoadFromString(Input));

    const auto assets = ComponentFactory::Create<Assets>("assets::assets3");
    EXPECT_TRUE(assets->Initialize(prop->Root()->Child("assets")));

    const auto accel = ComponentFactory::Create<Accel3>("Stub_Accel");
    const auto scene = ComponentFactory::Create<Scene3>("scene::scene3");
    ASSERT_TRUE(scene->Initialize(prop->Root()->Child("scene"), assets.get(), accel.get()));

    // Serialize the scene
    std::string serialized;
    {
        std::ostringstream ss(std::ios::binary);
        ASSERT_TRUE(scene->Serialize(ss));
        serialized = ss.str();
    }

    // Deserialize into a new scene instance
    auto scene2 = ComponentFactory::Create<Scene3>("scene::scene3");
    std::istringstream ss(serialized, std::ios::binary);
    ASSERT_TRUE(scene2->Deserialize(ss, { { "assets", assets.get() },{ "accel", accel.get() } }));

    // Check consistencies
    const auto* n1 = scene2->PrimitiveByID("n1");
    ASSERT_NE(nullptr, n1);
    EXPECT_EQ("n1", n1->id);
    const auto* n2 = scene2->PrimitiveByID("n2");
    ASSERT_NE(nullptr, n2);
    EXPECT_EQ("n2", n2->id);
    ASSERT_NE(nullptr, n2->bsdf);
    EXPECT_EQ("bsdf_1", n2->bsdf->ID());
    EXPECT_TRUE(ExpectVecNear(Vec3(42), n2->bsdf->Reflectance().v));
    ASSERT_NE(nullptr, n2->mesh);
    EXPECT_EQ("mesh_1", n2->mesh->ID());
    EXPECT_EQ(1, n2->mesh->NumVertices());
    EXPECT_TRUE(ExpectNear(1_f, n2->mesh->Positions()[0]));
    EXPECT_TRUE(ExpectNear(2_f, n2->mesh->Positions()[1]));
    EXPECT_TRUE(ExpectNear(3_f, n2->mesh->Positions()[2]));
}

// --------------------------------------------------------------------------------

// Missing `lightmetrica_scene` node
//TEST_F(SceneTest, InvalidrRootNode_Fail)
//{
//    const auto InvalidrRootNode_Fail_Input = TestUtils::MultiLineLiteral(R"x(
//    | a:
//    )x");
//
//    const auto prop = ComponentFactory::Create<PropertyTree>();
//    EXPECT_TRUE(prop->LoadFromString(InvalidrRootNode_Fail_Input));
//
//    const auto assets = ComponentFactory::Create<Assets>("Stub_Assets");
//    const auto accel = ComponentFactory::Create<Accel>("Stub_Accel");
//    const auto scene = ComponentFactory::Create<Scene>();
//    const auto err = TestUtils::ExtractLogMessage(TestUtils::CaptureStdout([&]()
//    {
//        ASSERT_TRUE(scene->Initialize(prop->Root(), assets.get(), accel.get()));
//        Logger::Flush();
//    }));
//    EXPECT_EQ("Missing 'lightmetrica_scene' node", err);
//}

// --------------------------------------------------------------------------------

// Missing `version` node
//TEST_F(SceneTest, MissingVersionNode_Fail)
//{
//    const auto MissingVersionNode_Fail_Input = TestUtils::MultiLineLiteral(R"x(
//    | lightmetrica_scene:
//    |   assets:
//    )x");
//
//    const auto prop = ComponentFactory::Create<PropertyTree>();
//    EXPECT_TRUE(prop->LoadFromString(MissingVersionNode_Fail_Input));
//
//    const auto assets = ComponentFactory::Create<Assets>("Stub_Assets");
//    const auto accel = ComponentFactory::Create<Accel>("Stub_Accel");
//    const auto scene = ComponentFactory::Create<Scene>();
//    const auto err = TestUtils::ExtractLogMessage(TestUtils::CaptureStdout([&]()
//    {
//        ASSERT_TRUE(scene->Initialize(prop->Root(), assets.get(), accel.get()));
//        Logger::Flush();
//    }));
//    EXPECT_EQ("Missing 'version' node", err);
//}

// --------------------------------------------------------------------------------

// Invalid version string
//TEST_F(SceneTest, InvalidVersionString_Fail)
//{
//    const auto InvalidVersionString_Fail_Input = TestUtils::MultiLineLiteral(R"x(
//    | lightmetrica_scene:
//    |   version: 1.0
//    )x");
//
//    const auto prop = ComponentFactory::Create<PropertyTree>();
//    EXPECT_TRUE(prop->LoadFromString(InvalidVersionString_Fail_Input));
//
//    const auto assets = ComponentFactory::Create<Assets>("Stub_Assets");
//    const auto accel = ComponentFactory::Create<Accel>("Stub_Accel");
//    const auto scene = ComponentFactory::Create<Scene>();
//    const auto err = TestUtils::ExtractLogMessage(TestUtils::CaptureStdout([&]()
//    {
//        ASSERT_TRUE(scene->Initialize(prop->Root(), assets.get(), accel.get()));
//        Logger::Flush();
//    }));
//    EXPECT_TRUE(boost::starts_with(err, "Invalid version string"));
//}

// --------------------------------------------------------------------------------

// Version check fails
//TEST_F(SceneTest, InvalidVersion_Fail)
//{
//    const auto InvalidVersion_Fail_Input = TestUtils::MultiLineLiteral(R"x(
//    | lightmetrica_scene:
//    |   version: 0.0.0
//    )x");
//
//    const auto prop = ComponentFactory::Create<PropertyTree>();
//    EXPECT_TRUE(prop->LoadFromString(InvalidVersion_Fail_Input));
//
//    const auto assets = ComponentFactory::Create<Assets>("Stub_Assets");
//    const auto accel = ComponentFactory::Create<Accel>("Stub_Accel");
//    const auto scene = ComponentFactory::Create<Scene>();
//    const auto err = TestUtils::ExtractLogMessage(TestUtils::CaptureStdout([&]()
//    {
//        ASSERT_TRUE(scene->Initialize(prop->Root(), assets.get(), accel.get()));
//        Logger::Flush();
//    }));
//    EXPECT_TRUE(boost::starts_with(err, "Invalid version"));
//}

// --------------------------------------------------------------------------------

// There is no `sensor` node
//TEST_F(SceneTest, NoSensor_Fail)
//{
//    const auto NoSensor_Fail_Input = TestUtils::MultiLineLiteral(R"x(
//    | nodes:
//    |   - id: n1
//    |   - id: n2
//    )x");
//
//    const auto prop = ComponentFactory::Create<PropertyTree>();
//    EXPECT_TRUE(prop->LoadFromString(NoSensor_Fail_Input));
//
//    const auto assets = ComponentFactory::Create<Assets>("Stub_Assets");
//    const auto accel = ComponentFactory::Create<Accel>("Stub_Accel");
//    const auto scene = ComponentFactory::Create<Scene>();
//    const auto err = TestUtils::ExtractLogMessage(TestUtils::CaptureStdout([&]()
//    {
//        ASSERT_FALSE(scene->Initialize(prop->Root(), assets.get(), accel.get()));
//        Logger::Flush();
//    }));
//    EXPECT_TRUE(boost::starts_with(err, "Missing 'sensor' node")) << err;
//}

// --------------------------------------------------------------------------------

// Invalid number of arguments in `transform`
//TEST_F(SceneTest, Transform_Fail)
//{
//    const auto Transform_Fail_Input = TestUtils::MultiLineLiteral(R"x(
//    | lightmetrica_scene:
//    |   version: 1.0.0
//    |   scene:
//    |     sensor: n1
//    |     nodes:
//    |       - id: n1
//    |         transform: {{transform}}
//    )x");
//
//    const std::string TransformNodes[] =
//    {
//        TestUtils::MultiLineLiteral(R"x(
//        | {}
//        )x")
//    };
//
//    FAIL();
//}

LM_TEST_NAMESPACE_END
