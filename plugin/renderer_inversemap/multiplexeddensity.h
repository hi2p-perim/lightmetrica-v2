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

#include "inversemaputils.h"

#define INVERSEMAP_MULTIPLEXED_DENSITY_DEBUG_SIMPLIFY_STRATEGY_SINGLE 0
#define INVERSEMAP_MULTIPLEXED_DENSITY_DEBUG_SIMPLIFY_STRATEGY_S0 0
#define INVERSEMAP_MULTIPLEXED_DENSITY_DEBUG_SIMPLIFY_STRATEGY_S1 0
#define INVERSEMAP_MULTIPLEXED_DENSITY_DEBUG_SIMPLIFY_STRATEGY_S2 0

LM_NAMESPACE_BEGIN

class MultiplexedDensity
{
public:

    LM_DISABLE_CONSTRUCT(MultiplexedDensity);

public:

    struct State
    {

        int numVertices_;
        Float uT_;                  // For technique selection
        std::vector<Float> usL_;    // For light subpath
        std::vector<Float> usE_;    // For eye subpath

    public:

        State() {}

        State(Random* rng, int numVertices)
            : numVertices_(numVertices)
        {
            // Consumes 3 random numbers for sampling a vertex
            const auto numStates = numVertices * 3;
            usL_.assign(numStates, 0_f);
            usE_.assign(numStates, 0_f);
            uT_ = rng->Next();
            for (auto& u : usE_) u = rng->Next();
            for (auto& u : usL_) u = rng->Next();
        }

        State(const State& o)
            : numVertices_(o.numVertices_)
            , uT_(o.uT_)
            , usL_(o.usL_)
            , usE_(o.usE_)
        {}

    public:

        auto ToVector() const -> std::vector<Float>
        {
            std::vector<Float> v;
            v.push_back(uT_);
            v.insert(v.end(), usL_.begin(), usL_.end());
            v.insert(v.end(), usE_.begin(), usE_.end());
			return v;
        }

        auto Swap(State& o) -> void
        {
            std::swap(uT_, o.uT_);
            usL_.swap(o.usL_);
            usE_.swap(o.usE_);
        }

    public:

        // Large step mutation
        auto LargeStep(Random* rng) const -> State
        {
            State next(*this);
            next.uT_ = rng->Next();
            for (auto& u : next.usE_) u = rng->Next();
            for (auto& u : next.usL_) u = rng->Next();
            return next;
        }

        // Small step mutation
        auto SmallStep(Random* rng, Float s1 = 1_f / 256_f, Float s2 = 1_f / 16_f) const -> State
        {
            State next(*this);
            next.uT_ = Perturb(rng, uT_, s1, s2);
            for (size_t i = 0; i < usE_.size(); i++) next.usE_[i] = Perturb(rng, usE_[i], s1, s2);
            for (size_t i = 0; i < usL_.size(); i++) next.usL_[i] = Perturb(rng, usL_[i], s1, s2);
            return next;
        }

        auto ChangeTechnique(Random* rng, Float s1 = 1_f / 256_f, Float s2 = 1_f / 16_f) const -> State
        {
            State next(*this);
            next.uT_ = Perturb(rng, uT_, s1, s2);
            return next;
        }

    private:

        auto Perturb(Random* rng, const Float u, Float s1, Float s2) const -> Float
        {
            Float result;
            Float r = rng->Next();
            if (r < 0.5_f)
            {
                r = r * 2_f;
                result = u + s2 * std::exp(-std::log(s2 / s1) * r);
                if (result > 1_f) result -= 1_f;
            }
            else
            {
                r = (r - 0.5_f) * 2_f;
                result = u - s2 * std::exp(-std::log(s2 / s1) * r);
                if (result < 0_f) result += 1_f;
            }
            return result;
        };

    };

public:

    struct CachedPath
    {
        int s, t;
        Path path;
        SPD Cstar;      // Caches unweighed contribution
        Float w;        // Caches MIS weight
    };
    static auto InvCDF(const State& s, const Scene3* scene) -> boost::optional<CachedPath>
    {
        Subpath subpathE;
        Subpath subpathL;
        subpathE.vertices.clear();
        subpathL.vertices.clear();
        subpathE.SampleSubpathWithPrimarySamples(scene, s.usE_, TransportDirection::EL, s.numVertices_);
        subpathL.SampleSubpathWithPrimarySamples(scene, s.usL_, TransportDirection::LE, s.numVertices_);

        CachedPath p;
        #if INVERSEMAP_MULTIPLEXED_DENSITY_DEBUG_SIMPLIFY_STRATEGY_SINGLE
        #if INVERSEMAP_MULTIPLEXED_DENSITY_DEBUG_SIMPLIFY_STRATEGY_S0
        p.t = s.numVertices_;
        p.s = 0;
        #elif INVERSEMAP_MULTIPLEXED_DENSITY_DEBUG_SIMPLIFY_STRATEGY_S1
        p.t = s.numVertices_-1;
        p.s = 1;
        #elif INVERSEMAP_MULTIPLEXED_DENSITY_DEBUG_SIMPLIFY_STRATEGY_S2
        p.t = s.numVertices_ - 2;
        p.s = 2;
        #else
        #error "unreachable"
        #endif
        #else
        p.t = Math::Min(s.numVertices_, (int)(s.uT_ * (s.numVertices_ + 1)));
        p.s = s.numVertices_ - p.t;
        #endif

        const int nE = (int)(subpathE.vertices.size());
        const int nL = (int)(subpathL.vertices.size());
        if (p.t > nE || p.s > nL) { return boost::none; }

        //if (p.t < 0 || p.s < 0)
        //{
        //    std::cout << p.t << std::endl;
        //    std::cout << p.s << std::endl;
        //    std::cout << s.numVertices_ << std::endl;
        //    std::cout.flush();
        //    __debugbreak();
        //}

        if (!p.path.ConnectSubpaths(scene, subpathL, subpathE, p.s, p.t)) { return boost::none; }
        p.Cstar = p.path.EvaluateUnweightContribution(scene, p.s);
        if (p.Cstar.Black()) { return boost::none; }

        p.w = p.path.EvaluateMISWeight(scene, p.s);
        return p;
    }

    // Maps a path to a state in multiplexed primary sample space
    static auto CDF(const Path& p, int s, const Scene3* scene, Random* rng) -> boost::optional<State>
    {
        const int n = (int)(p.vertices.size());
        const int t = n - s;

        // This ensures uninitialized parts is filled with fresh random numbers
        State state(rng, n);

        // Map subpaths
        const auto usL = CDF_Subpath(scene, p, s, rng, TransportDirection::LE);
        assert(usL.size() <= state.usL_.size());
        for (size_t i = 0; i < usL.size(); i++) { state.usL_[i] = usL[i]; }
        const auto usE = CDF_Subpath(scene, p, t, rng, TransportDirection::EL);
        assert(usE.size() <= state.usE_.size());
        for (size_t i = 0; i < usE.size(); i++) { state.usE_[i] = usE[i]; }

        // Map technique
        state.uT_ = Math::Clamp(((Float)t + rng->Next()) / (n + 1), 0_f, 1_f);

        return state;
    }

    static auto CDF_Subpath(const Scene3* scene, const Path& p, int k, Random* rng, TransportDirection transDir) -> std::vector<Float>
    {
        const int n = (int)(p.vertices.size());
        const auto index = [&](int i) -> int { return transDir == TransportDirection::LE ? i : n - 1 - i; };
        const auto undef = [&]() -> Float { return rng->Next(); };

        std::vector<Float> us;
        for (int i = 0; i < k; i++)
        {
            const auto* v = &p.vertices[index(i)];
            const auto* vp = index(i - 1) >= 0 && index(i - 1) < n ? &p.vertices[index(i - 1)] : nullptr;
            const auto* vpp = index(i - 2) >= 0 && index(i - 2) < n ? &p.vertices[index(i - 2)] : nullptr;
            //assert(vp != nullptr);

            if (!vp)
            {
                assert(i == 0);
                if (transDir == TransportDirection::EL)
                {
                    // Pinhole camera
                    assert(std::strcmp(v->primitive->sensor->implName, "Sensor_Pinhole") == 0);
                    us.push_back(undef());
                    us.push_back(undef());
                    us.push_back(undef());
                }
                else
                {
                    // Area light
                    assert(std::strcmp(v->primitive->emitter->implName, "Light_Area") == 0);
                    const auto* triAreaDist = v->primitive->light->TriAreaDist();
                    const auto u = InversemapUtils::SampleTriangleMesh_Inverse(v->primitive, *triAreaDist, v->geom);
                    us.push_back(u[0]);
                    us.push_back(u[1]);

                    // Light selection prob
                    const auto uC = Math::Clamp((rng->Next() + (Float)v->primitive->lightIndex) / scene->NumLightPrimitives(), 0_f, 1_f);
                    us.push_back(uC);
                }
            }
            else if (vp->type == SurfaceInteractionType::E)
            {
                const auto wo = Math::Normalize(v->geom.p - vp->geom.p);
                Vec2 u;
                vp->primitive->sensor->RasterPosition(wo, vp->geom, u);
                us.push_back(u[0]);
                us.push_back(u[1]);
                us.push_back(undef());
            }
            else if (vp->type == SurfaceInteractionType::L)
            {
                const auto wo = Math::Normalize(v->geom.p - vp->geom.p);
                const auto localWo = vp->geom.ToLocal * wo;
                const auto u = InversemapUtils::UniformConcentricDiskSample_Inverse(Vec2(localWo.x, localWo.y));
                us.push_back(u[0]);
                us.push_back(u[1]);
                us.push_back(undef());
            }
            else if (vp->type == SurfaceInteractionType::D)
            {
                const auto wo = Math::Normalize(v->geom.p - vp->geom.p);
                const auto localWo = vp->geom.ToLocal * wo;
                const auto u = InversemapUtils::UniformConcentricDiskSample_Inverse(Vec2(localWo.x, localWo.y));
                us.push_back(u[0]);
                us.push_back(u[1]);
                us.push_back(undef());
            }
            else if (vp->type == SurfaceInteractionType::G)
            {
                const auto wo = Math::Normalize(v->geom.p - vp->geom.p);
                const auto wi = Math::Normalize(vpp->geom.p - vp->geom.p);
                const auto localWo = vp->geom.ToLocal * wo;
                const auto localWi = vp->geom.ToLocal * wi;
                const auto H = Math::Normalize(localWi + localWo);
                const auto roughness = vp->primitive->bsdf->Glossiness();
                const auto u = InversemapUtils::SampleGGX_Inverse(roughness, H);
                us.push_back(u[0]);
                us.push_back(u[1]);
                us.push_back(undef());
            }
            else if (vp->type == SurfaceInteractionType::S)
            {
                if (std::strcmp(vp->primitive->bsdf->implName, "BSDF_ReflectAll") == 0)
                {
                    us.push_back(undef());
                    us.push_back(undef());
                    us.push_back(undef());
                }
                else if (std::strcmp(vp->primitive->bsdf->implName, "BSDF_RefractAll") == 0)
                {
                    us.push_back(undef());
                    us.push_back(undef());
                    us.push_back(undef());
                }
                else if (std::strcmp(vp->primitive->bsdf->implName, "BSDF_Flesnel") == 0)
                {
                    const auto wo = Math::Normalize(v->geom.p - vp->geom.p);
                    const auto wi = Math::Normalize(vpp->geom.p - vp->geom.p);
                    const auto localWo = vp->geom.ToLocal * wo;
                    const auto localWi = vp->geom.ToLocal * wi;
                    const auto Fr = vp->primitive->bsdf->FlesnelTerm(vp->geom, wi);
                    us.push_back(undef());
                    us.push_back(undef());
                    if (Math::LocalCos(localWi) * Math::LocalCos(localWo) >= 0_f)
                    {
                        // Reflection: Set u <= Fr
                        us.push_back(rng->Next() * (Fr - Math::Eps()));
                    }
                    else
                    {
                        // Refraction: Set u > Fr
                        us.push_back(Math::Eps() + Fr + rng->Next() * (1_f - Fr - Math::Eps()));
                    }
                }
                else
                {
                    LM_UNREACHABLE();
                }
            }
            else
            {
                LM_UNREACHABLE();
            }

            assert(us.size() % 3 == 0);
        }

        assert(us.size() == k * 3);
        return us;
    }

};

LM_NAMESPACE_END
