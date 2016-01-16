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
#include <lightmetrica/ray.h>
#include <lightmetrica/intersection.h>
#include <lightmetrica-test/mathutils.h>

LM_TEST_NAMESPACE_BEGIN

struct AccelTest : public ::testing::TestWithParam<const char*>
{
    virtual auto SetUp() -> void override { Logger::SetVerboseLevel(2); Logger::Run(); }
    virtual auto TearDown() -> void override { Logger::Stop(); }
};

INSTANTIATE_TEST_CASE_P(AccelTypes, AccelTest, ::testing::Values("naiveaccel", "embree"));

// --------------------------------------------------------------------------------

#pragma region Stub triangle mesh

// {(x, y, z) : 0<=x,y<=1, z=0,-1}
class StubTriangleMesh_Simple : public TriangleMesh
{
public:

    LM_IMPL_CLASS(StubTriangleMesh_Simple, TriangleMesh);

public:

    LM_IMPL_F(NumVertices) = [this]() -> int { return (int)(ps.size()); };
    LM_IMPL_F(NumFaces)    = [this]() -> int { return (int)(fs.size()); };
    LM_IMPL_F(Positions)   = [this]() -> const Float* { return ps.data(); };
    LM_IMPL_F(Normals)     = [this]() -> const Float* { return ns.data(); };
    LM_IMPL_F(Texcoords)   = [this]() -> const Float* { return ts.data(); };
    LM_IMPL_F(Faces)       = [this]() -> const unsigned int* { return fs.data(); };

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

class StubTriangleMesh_Simple2 : public TriangleMesh
{
public:

    LM_IMPL_CLASS(StubTriangleMesh_Simple2, TriangleMesh);

public:

    LM_IMPL_F(NumVertices) = [this]() -> int { return (int)(ps.size()); };
    LM_IMPL_F(NumFaces)    = [this]() -> int { return (int)(fs.size()); };
    LM_IMPL_F(Positions)   = [this]() -> const Float* { return ps.data(); };
    LM_IMPL_F(Normals)     = [this]() -> const Float* { return ns.data(); };
    LM_IMPL_F(Texcoords)   = [this]() -> const Float* { return ts.data(); };
    LM_IMPL_F(Faces)       = [this]() -> const unsigned int* { return fs.data(); };

protected:

    std::vector<Float> ps{
        0, 0, 0,
        1, 0, -1,
        1, 1, -1,
        0, 1, 0
    };
    std::vector<Float> ns{
        0.707106781186547_f, 0_f, 0.707106781186547_f,
        0.707106781186547_f, 0_f, 0.707106781186547_f,
        0.707106781186547_f, 0_f, 0.707106781186547_f,
        0.707106781186547_f, 0_f, 0.707106781186547_f,
    };
    std::vector<Float> ts{
        0, 0,
        1, 0,
        1, 1,
        0, 1
    };
    std::vector<unsigned int> fs{
        0, 1, 2,
        0, 2, 3
    };

};

// Many triangles in [0, 1]^3
class StubTriangleMesh_Random : public TriangleMesh
{
public:

    LM_IMPL_CLASS(StubTriangleMesh_Random, TriangleMesh);

public:

    LM_IMPL_F(NumVertices) = [this]() -> int { return (int)(ps.size()); };
    LM_IMPL_F(NumFaces)    = [this]() -> int { return (int)(fs.size()); };
    LM_IMPL_F(Positions)   = [this]() -> const Float* { return ps.data(); };
    LM_IMPL_F(Normals)     = [this]() -> const Float* { return ns.data(); };
    LM_IMPL_F(Texcoords)   = [this]() -> const Float* { return ts.data(); };
    LM_IMPL_F(Faces)       = [this]() -> const unsigned int* { return fs.data(); };

public:

    StubTriangleMesh_Random()
    {
        // Fix seed
        std::mt19937 gen(42);
        std::uniform_real_distribution<double> dist;

        const int FaceCount = 1000;
        for (int i = 0; i < FaceCount; i++)
        {
            auto p1 = Vec3(Float(dist(gen)), Float(dist(gen)), Float(dist(gen)));
            auto p2 = Vec3(Float(dist(gen)), Float(dist(gen)), Float(dist(gen)));
            auto p3 = Vec3(Float(dist(gen)), Float(dist(gen)), Float(dist(gen)));

            ps.push_back(p1[0]);
            ps.push_back(p1[1]);
            ps.push_back(p1[2]);
            ps.push_back(p2[0]);
            ps.push_back(p2[1]);
            ps.push_back(p2[2]);
            ps.push_back(p3[0]);
            ps.push_back(p3[1]);
            ps.push_back(p3[2]);

            auto n = Math::Cross(p2 - p1, p3 - p1);
            for (int j = 0; j < 3; j++)
            {
                ns.push_back(n[0]);
                ns.push_back(n[1]);
                ns.push_back(n[2]);
            }

            fs.push_back(3 * i + 2);
        }
    }

private:

    std::vector<Float> ps;
    std::vector<Float> ns;
    std::vector<Float> ts;
    std::vector<unsigned int> fs;

};

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

    Stub_Scene(const TriangleMesh& mesh)
    {
        primitive_.transform = Mat4::Identity();
        primitive_.mesh = &mesh;
    }

private:

    Primitive primitive_;

};

#pragma endregion

// --------------------------------------------------------------------------------

TEST_P(AccelTest, Simple)
{
    StubTriangleMesh_Simple mesh;
    Stub_Scene scene(mesh);

    const auto accel = ComponentFactory::Create<Accel>(GetParam());
    EXPECT_TRUE(accel->Initialize(nullptr));
    EXPECT_TRUE(accel->Build(scene));

    // Trace rays in the region of [0, 1]^2
    Ray ray;
    Intersection isect;
    const int Steps = 10;
    const Float Delta = 1_f / Float(Steps);
    for (int i = 1; i < Steps; i++)
    {
        const Float y = Delta * Float(i);
        for (int j = 1; j < Steps; j++)
        {
            const Float x = Delta * Float(j);

            // Intersection query
            ray.o = Vec3(0, 0, 1);
            ray.d = Math::Normalize(Vec3(x, y, 0) - ray.o);

            ASSERT_TRUE(accel->Intersect(scene, ray, isect, 0_f, Math::Inf()));
            EXPECT_TRUE(ExpectVecNear(Vec3(x, y, 0), isect.geom.p,  Math::EpsLarge()));
            EXPECT_TRUE(ExpectVecNear(Vec3(0, 0, 1), isect.geom.gn, Math::EpsLarge()));
            EXPECT_TRUE(ExpectVecNear(Vec3(0, 0, 1), isect.geom.sn, Math::EpsLarge()));
            EXPECT_TRUE(ExpectVecNear(Vec2(x, y),    isect.geom.uv, Math::EpsLarge()));
        }
    }
}

TEST_P(AccelTest, Simple2)
{
    StubTriangleMesh_Simple2 mesh;
    Stub_Scene scene(mesh);

    const auto accel = ComponentFactory::Create<Accel>(GetParam());
    EXPECT_TRUE(accel->Initialize(nullptr));
    EXPECT_TRUE(accel->Build(scene));

    // Trace rays in the region of [0, 1]^2
    Ray ray;
    Intersection isect;
    const int Steps = 10;
    const Float Delta = 1_f / Float(Steps);
    for (int i = 1; i < Steps; i++)
    {
        const Float y = Delta * Float(i);
        for (int j = 1; j < Steps; j++)
        {
            const Float x = Delta * Float(j);

            // Intersection query
            ray.o = Vec3(x, y, 1);
            ray.d = Vec3(0, 0, -1);

            ASSERT_TRUE(accel->Intersect(scene, ray, isect, 0_f, Math::Inf()));
            EXPECT_TRUE(ExpectVecNear(Vec3(x, y, -x), isect.geom.p, Math::EpsLarge()));
            EXPECT_TRUE(ExpectVecNear(Math::Normalize(Vec3(1, 0, 1)), isect.geom.gn, Math::EpsLarge()));
            EXPECT_TRUE(ExpectVecNear(Math::Normalize(Vec3(1, 0, 1)), isect.geom.sn, Math::EpsLarge()));
            EXPECT_TRUE(ExpectVecNear(Vec2(x, y), isect.geom.uv, Math::EpsLarge()));
        }
    }
}

LM_TEST_NAMESPACE_END
