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
#include <lightmetrica/logger.h>
#include <lightmetrica/exception.h>
#include <lightmetrica/fp.h>
#include <lightmetrica/random.h>
#include <lightmetrica/math.h>
#include <lightmetrica-test/mathutils.h>

#define LM_INVERSEMAP_TEST_OUTPUT_FAILED_H 0

LM_TEST_NAMESPACE_BEGIN

struct InversemapTest : public ::testing::Test
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

public:

    const Float roughness_ = 0.1_f;
    const long long NumSamples = 10000;

#if 0
    auto EvaluateBechmannDist(const Vec3& H, Float roughness) const -> Float
    {
        if (Math::LocalCos(H) <= 0_f) return 0_f;
        const Float ex = Math::LocalTan(H) / roughness;
        const Float t1 = std::exp(-(ex * ex));
        const Float t2 = (Math::Pi() * roughness * roughness * std::pow(Math::LocalCos(H), 4_f));
        return t1 / t2;
    }

    auto SampleBechmannDist(const Vec2& u) const -> Vec3
    {
        const Float cosThetaH = [&]() -> Float
        {
            // Handling nasty numerical error
            if (1_f - u[0] < Math::Eps()) return 0_f;
            const Float tanThetaHSqr = -roughness_ * roughness_ * std::log(1_f - u[0]);
            return 1_f / Math::Sqrt(1_f + tanThetaHSqr);
        }();
        const Float cosThetaH2 = cosThetaH * cosThetaH;
        Float sinThetaH = Math::Sqrt(Math::Max(0_f, 1_f - cosThetaH2));
        Float phiH = 2_f * Math::Pi() * u[1];
        return Vec3(sinThetaH * Math::Cos(phiH), sinThetaH * Math::Sin(phiH), cosThetaH);
    }

    auto SampleBechmannDist_Inverse(const Vec3& H) const -> Vec2
    {
        const auto u0 = [&]() -> Float
        {
            const auto cosThetaH = Math::LocalCos(H);
            if (cosThetaH * cosThetaH < Math::Eps()) return 1_f;
            const auto tanThetaHSqr = 1_f / (cosThetaH * cosThetaH) - 1_f;
            const auto exp = std::exp(-tanThetaHSqr / (roughness_ * roughness_));
            return 1_f - exp;
        }();

        const auto sinThetaH = Math::LocalSin(H);
        const auto cosPhiH = H.x / sinThetaH;
        const auto sinPhiH = H.y / sinThetaH;
        const auto phiH = [&]() {
            const auto t = std::atan2(sinPhiH, cosPhiH);
            return t < 0_f ? t + 2_f * Math::Pi() : t;
        }();
        const auto u1 = phiH * 0.5_f * Math::InvPi();

        return Vec2(u0, u1);
    }
#endif

    auto EvalGGX(const Vec3& H, Float roughness) -> Float
    {
        const auto cosH = Math::LocalCos(H);
        const auto tanH = Math::LocalTan(H);
        if (cosH <= 0_f) return 0_f;
        const Float t1 = roughness * roughness;
        const Float t2 = Math::Pi() * cosH * cosH * cosH * cosH * (roughness * roughness + tanH * tanH);
        return t1 / t2;
    }

    auto SampleGGX(const Vec2& u) -> Vec3
    {
        // Input u \in [0,1]^2
        const auto ToOpenOpen = [](Float u) -> Float { return (1_f - 2_f * Math::Eps()) * u + Math::Eps(); };
        //const auto ToClosedOpen = [](Float u) -> Float { return (1_f - Math::Eps()) * u; };
        const auto ToOpenClosed = [](Float u) -> Float { return (1_f - Math::Eps()) * u + Math::Eps(); };

        // u0 \in (0,1]
        // u1 \in (0,1)
        const auto u0 = ToOpenClosed(u[0]);
        const auto u1 = ToOpenOpen(u[1]);

        // Robust way of computation
        const auto cosTheta = [&]() -> Float {
            const auto v1 = Math::Sqrt(1_f - u0);
            const auto v2 = Math::Sqrt(1_f - (1_f - roughness_ * roughness_) * u0);
            return v1 / v2;
        }();
        const auto sinTheta = [&]() -> Float {
            const auto v1 = Math::Sqrt(u0);
            const auto v2 = Math::Sqrt(1_f - (1_f - roughness_ * roughness_) * u0);
            return roughness_ * (v1 / v2);
        }();
        const auto phi = Math::Pi() * (2_f * u1 - 1_f);
        return Vec3(sinTheta * Math::Cos(phi), sinTheta * Math::Sin(phi), cosTheta);
    }

    auto SampleGGX_Inverse(const Vec3& H) -> Vec2
    {
        const auto tanTheta2 = Math::LocalTan2(H);
        const auto u0 = 1_f / (1_f + roughness_ * roughness_ / tanTheta2);

        const auto phiH = [&]() {
            const auto t = std::atan2(H.y, H.x);
            return t;
        }();
        const auto u1 = (phiH * Math::InvPi() + 1_f) * 0.5_f;

        return Vec2(u0, u1);
    }

};

// --------------------------------------------------------------------------------

#if 0
// Tests if CDF(CDF^-1(u)) = u for Bechmann distribution
TEST_F(InversemapTest, BeckmannDistInverseConsistency)
{
    Random rng;
    rng.SetSeed(42);

    for (long long i = 0; i < NumSamples; i++)
    {
        SCOPED_TRACE("Sample: " + std::to_string(i));

        const auto u = rng.Next2D();

        // H := CDF^-1(u)
        const auto H = SampleBechmannDist(u);

        // u2 := CDF(H)
        const auto u2 = SampleBechmannDist_Inverse(H);

        EXPECT_TRUE(ExpectVecNear(u, u2, Math::Eps()));
    }
}

// Tests if CDF^-1(CDF(H)) = H for Bechmann distribution
TEST_F(InversemapTest, BeckmannDistInverseConsistencyInv)
{
    Random rng;
    rng.SetSeed(42);

    for (long long i = 0; i < NumSamples; i++)
    {
        SCOPED_TRACE("Sample: " + std::to_string(i));

        const auto H = Math::Normalize(Vec3(2_f * rng.Next() - 0.5_f, 2_f * rng.Next() - 0.5_f, rng.Next()));

        // u := CDF(H)
        const auto u = SampleBechmannDist_Inverse(H);

        // H2 := CDF^-1(u)
        const auto H2 = SampleBechmannDist(u);

        const auto result = ExpectVecNear(H, H2, Math::EpsLarge());
        EXPECT_TRUE(result);

        #if LM_INVERSEMAP_TEST_OUTPUT_FAILED_H
        if (!result)
        {
            static long long count = 0;
            if (count == 0)
            {
                boost::filesystem::remove("H.out");
                boost::filesystem::remove("H2.out");
            }
            if (count < 10)
            {
                count++;
                {
                    std::ofstream out("H.out", std::ios::out | std::ios::app);
                    out << boost::str(boost::format("0 0 0 %.10f %.10f %.10f ") % H.x % H.y % H.z);
                    out << std::endl;
                }
                {
                    std::ofstream out("H2.out", std::ios::out | std::ios::app);
                    out << boost::str(boost::format("0 0 0 %.10f %.10f %.10f ") % H2.x % H2.y % H2.z);
                    out << std::endl;
                }
            }
        }
        else
        {
            static long long count = 0;
            if (count == 0)
            {
                boost::filesystem::remove("H_good.out");
            }
            if (count < 10)
            {
                count++;
                {
                    std::ofstream out("H_good.out", std::ios::out | std::ios::app);
                    out << boost::str(boost::format("0 0 0 %.10f %.10f %.10f ") % H.x % H.y % H.z);
                    out << std::endl;
                }
            }
        }
        #endif
    }
}
#endif

// --------------------------------------------------------------------------------

// Tests if CDF(CDF^-1(u)) = u for GGX
TEST_F(InversemapTest, GGXInverseConsistency)
{
    try
    {
        Random rng;
        rng.SetSeed(42);

        for (long long i = 0; i < NumSamples; i++)
        {
            SCOPED_TRACE("Sample: " + std::to_string(i));

            const auto u = rng.Next2D();

            // H := CDF^-1(u)
            const auto H = SampleGGX(u);

            // u2 := CDF(H)
            const auto u2 = SampleGGX_Inverse(H);

            const auto result = ExpectVecNear(u, u2, 0.001_f);
            EXPECT_TRUE(result);
            if (!result)
            {
                //__debugbreak();
            }
        }
    }
    catch (const std::exception)
    {
        //__debugbreak();
    }
}

// --------------------------------------------------------------------------------

// Tests if CDF^-1(CDF(H)) = H for GGX
#if 1
TEST_F(InversemapTest, GGXInverseConsistencyInv)
{
    try
    {
        Random rng;
        rng.SetSeed(42);

        for (long long i = 0; i < NumSamples; i++)
        {
            SCOPED_TRACE("Sample: " + std::to_string(i));

            const auto H = Math::Normalize(Vec3(2_f * rng.Next() - 0.5_f, 2_f * rng.Next() - 0.5_f, rng.Next()));
            if (H.z < 0.5_f) continue;

            // u := CDF(H)
            const auto u = SampleGGX_Inverse(H);

            // H2 := CDF^-1(u)
            const auto H2 = SampleGGX(u);

            const auto result = Math::Abs(Math::Abs(Math::Dot(H, H2)) - 1_f) < 0.001_f;
            EXPECT_TRUE(result);
            if (!result)
            {
                // u2 := CDF(H2)
                const auto u2 = SampleGGX_Inverse(H2);
                LM_UNUSED(u2);
                //__debugbreak();
            }
        }
    }
    catch (const std::exception)
    {
        //__debugbreak();
    }
}
#endif

LM_TEST_NAMESPACE_END
