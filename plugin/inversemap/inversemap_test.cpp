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

#define LM_INVERSEMAP_TEST_OUTPUT_FAILED_H 1

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


    const Float roughness_ = 0.1_f;
    const long long NumSamples = 100;


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
    };

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
    };

};

// Tests if CDF(CDF^-1(u)) = u
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

// Tests if CDF^-1(CDF(H)) = H
TEST_F(InversemapTest, BeckmannDistInverseConsistency2)
{
    try
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
    catch (const std::runtime_error&)
    {
        LM_LOG_INFO("exception");
    }
}

LM_TEST_NAMESPACE_END
