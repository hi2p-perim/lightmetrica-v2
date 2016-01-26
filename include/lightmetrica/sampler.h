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

#pragma once

#include <lightmetrica/math.h>

LM_NAMESPACE_BEGIN

class Sampler
{
public:

    LM_DISABLE_CONSTRUCT(Sampler);

public:

    static auto LocalReflect(const Vec3& wi) -> Vec3
    {
        return Vec3(-wi.x, -wi.y, wi.z);
    }

    static auto LocalRefract(const Vec3& wi, Float eta, Float cosThetaT) -> Vec3
    {
        return Vec3(-eta * wi.x, -eta * wi.y, cosThetaT);
    }

    static auto UniformConcentricDiskSample(const Vec2& u) -> Vec2
    {
        auto v = 2_f * u - Vec2(1_f);
        if (v.x == 0_f && v.y == 0_f) { return Vec2(); }
        Float r, theta;
        if (v.x > -v.y)
        {
            if (v.x > v.y) { r = v.x; theta = (Math::Pi() * 0.25_f) * v.y / v.x; }
            else { r = v.y; theta = (Math::Pi() * 0.25_f) * (2_f - v.x / v.y); }
        }
        else
        {
            if (v.x < v.y) { r = -v.x; theta = (Math::Pi() * 0.25_f) * (4_f + v.y / v.x); }
            else { r = -v.y; theta = (Math::Pi() * 0.25_f) * (6_f - v.x / v.y); }
        }
        return Vec2(r * Math::Cos(theta), r * Math::Sin(theta));
    }

    static auto CosineSampleHemisphere(const Vec2& u) -> Vec3
    {
        const auto s = UniformConcentricDiskSample(u);
        return Vec3(s.x, s.y, Math::Sqrt(Math::Max(0_f, 1_f - s.x*s.x - s.y*s.y)));
    }

    static auto CosineSampleHemispherePDFProjSA(const Vec3& d) -> Float
    {
        return Math::InvPi();
    }

    static auto UniformSampleSphere(const Vec2& u) -> Vec3
    {
        const Float z = 1_f - 2_f * u[0];
        const Float r = Math::Sqrt(Math::Max(0_f, 1_f - z*z));
        const Float phi = 2_f * Math::Pi() * u[1];
        return Vec3(r * Math::Cos(phi), r * Math::Sin(phi), z);
    }

    static auto UniformSampleSpherePDFSA(const Vec3& d) -> Float
    {
        return Math::InvPi() * 0.25_f;
    }

    static auto UniformSampleTriangle(const Vec2& u) -> Vec2
    {
        const auto s = Math::Sqrt(Math::Max(0_f, u.x));
        return Vec2(1_f - s, u.y * s);
    }

};

LM_NAMESPACE_END
