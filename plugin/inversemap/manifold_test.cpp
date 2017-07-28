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
#include <lightmetrica/math.h>
#include <lightmetrica/bsdf.h>
#include <lightmetrica-test/mathutils.h>

#include "manifoldutils.h"

LM_TEST_NAMESPACE_BEGIN

struct ManifoldTest : public ::testing::Test
{

    virtual auto SetUp() -> void override
    {
        SEHUtils::EnableStructuralException();
        FPUtils::EnableFPControl();
        Logger::Run();
    }

    virtual auto TearDown() -> void override
    {
        Logger::Stop();
        FPUtils::DisableFPControl();
        SEHUtils::DisableStructuralException();
    }

};

// --------------------------------------------------------------------------------

// Stubs
struct StubS : public BSDF
{
    LM_IMPL_CLASS(StubS, BSDF);
    LM_IMPL_F(Eta) = [this](const SurfaceGeometry& geom, const Vec3& wi) -> Float { return 1_f; };
};

// --------------------------------------------------------------------------------

TEST_F(ManifoldTest, ExampleFromJacob2012)
{
    std::unique_ptr<StubS> stubS(new StubS);
    std::unique_ptr<Primitive> S(new Primitive);
    S->bsdf = stubS.get();

    Subpath subpath;
    {
        SubpathSampler::PathVertex v;
        v.type = SurfaceInteractionType::D;
        v.geom.degenerated = false;
        v.geom.p = Vec3(-1_f, 2_f, 0_f);
        v.geom.sn = v.geom.sn = Vec3(0_f,-1_f,0_f);
        v.geom.dpdu = Vec3(-1_f, 0_f, 0_f);
        v.geom.dpdv = Vec3(0_f, 0_f, 1_f);
        v.geom.dndu = Vec3();
        v.geom.dndv = Vec3();
        subpath.vertices.push_back(v);
    }
    {
        SubpathSampler::PathVertex v;
        v.type = SurfaceInteractionType::S;
        v.primitive = S.get();
        v.geom.degenerated = false;
        v.geom.p = Vec3(0_f, 1_f, 0_f);
        v.geom.sn = v.geom.sn = Vec3(0_f, 1_f, 0_f);
        v.geom.dpdu = Vec3(1_f, 0_f, 0_f);
        v.geom.dpdv = Vec3(0_f, 0_f, 1_f);
        v.geom.dndu = Vec3(1_f, 0_f, 0_f);
        v.geom.dndv = Vec3();
        subpath.vertices.push_back(v);
    }
    {
        SubpathSampler::PathVertex v;
        v.type = SurfaceInteractionType::D;
        v.geom.degenerated = false;
        v.geom.p = Vec3(1_f, 2_f, 0_f);
        v.geom.sn = v.geom.sn = Vec3(0_f, -1_f, 0_f);
        v.geom.dpdu = Vec3(1_f, 0_f, 0_f);
        v.geom.dpdv = Vec3(0_f, 0_f, 1_f);
        v.geom.dndu = Vec3();
        v.geom.dndv = Vec3();
        subpath.vertices.push_back(v);
    }
    
    ConstraintJacobian C(1);
    ManifoldUtils::ComputeConstraintJacobian(subpath, C);

    {
        const Mat2 expected{
            -1_f / 4_f, 0_f,
            0_f, 1_f / 2_f
        };
        EXPECT_TRUE(ExpectMatNear(expected, C[0].A));
    }
    {
        const Mat2 expected{
            -3_f / 2_f, 0_f,
            0_f, -1_f
        };
        EXPECT_TRUE(ExpectMatNear(expected, C[0].B));
    }
    {
        const Mat2 expected{
            1_f / 4_f, 0_f,
            0_f, 1_f / 2_f
        };
        EXPECT_TRUE(ExpectMatNear(expected, C[0].C));
    }

    {
        const auto G = RenderUtils::GeometryTerm(subpath.vertices[0].geom, subpath.vertices[1].geom);
        const auto multiG = ManifoldUtils::ComputeConstraintJacobianDeterminant(subpath);
        const auto invdet = 1_f / (G * multiG);
        EXPECT_TRUE(ExpectNear(48_f, invdet));
    }
}

LM_TEST_NAMESPACE_END
