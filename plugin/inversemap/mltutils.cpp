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

#include "mltutils.h"
#include "inversemaputils.h"
#include "manifoldutils.h"
#include "debugio.h"
#include <regex>
#include <atomic>
#include <cereal/archives/json.hpp>
#include <cereal/types/vector.hpp>

LM_NAMESPACE_BEGIN

namespace
{

    auto Perturb(Random& rng, const Float u, const Float s1, const Float s2)
    {
        Float result;
        Float r = rng.Next();
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
    }

    auto PerturbRasterPos(const Path& currP, Random& rng, Float s1, Float s2) -> boost::optional<Vec2>
    {
        // Calculating raster position from the path have small correlated error so just iterating
        // update can change the state of the path. This affect the mixing of the chain especially when
        // the kernel size is relatively small. However for moderately bigger kernels, this effect is negilible.
        // Essentially this can happen with technique with inverse mapping, because calculating raster position
        // is a process of inverse of CDF^-1 for the directing sampling of the camera rays.
        const auto rasterPos = currP.RasterPosition();
        //const auto s1 = 1_f / 256_f;
        //const auto s2 = 1_f / 16_f;
        const auto rX = Perturb(rng, rasterPos.x, s1, s2);
        const auto rY = Perturb(rng, rasterPos.y, s1, s2);
        // Immediately reject if the proposed raster position is outside of [0,1]^2
        if (rX < 0_f || rX > 1_f || rY < 0_f || rY > 1_f)
        {
            return boost::none;
        }
        return Vec2(rX, rY);
    }

    auto PerturbDSample(const Path& currP, Random& rng, int i, TransportDirection transDir, Float s1, Float s2) -> boost::optional<Vec2>
    {
        // Current sample for direction sampling
        const auto currU = [&]() -> Vec2
        {
            const int n = (int)(currP.vertices.size());
            const auto index = [&](int i_)
            {
                return transDir == TransportDirection::LE ? i_ : n - 1 - i_;
            };

            const auto* v = &currP.vertices[index(i)];
            const auto* vn = &currP.vertices[index(i + 1)];
            const auto* vp = index(i - 1) >= 0 && index(i - 1) < n ? &currP.vertices[index(i - 1)] : nullptr;
            const auto wo = Math::Normalize(vn->geom.p - v->geom.p);
            const auto wi = vp ? Math::Normalize(vp->geom.p - v->geom.p) : Vec3();
            if (v->type == SurfaceInteractionType::D || v->type == SurfaceInteractionType::L)
            {
                const auto localWo = v->geom.ToLocal * wo;
                return InversemapUtils::UniformConcentricDiskSample_Inverse(Vec2(localWo.x, localWo.y));
            }
            else if (v->type == SurfaceInteractionType::G)
            {
                const auto localWi = v->geom.ToLocal * wi;
                const auto localWo = v->geom.ToLocal * wo;
                const auto H = Math::Normalize(localWi + localWo);
                const auto roughness = v->primitive->bsdf->Glossiness();
                return InversemapUtils::SampleGGX_Inverse(roughness, H);
            }
            LM_UNREACHABLE();
            return Vec2();
        }();

        // Perturb it
        //const auto s1 = 1_f / 256_f;
        //const auto s2 = 1_f / 16_f;
        const auto u1 = Perturb(rng, currU.x, s1, s2);
        const auto u2 = Perturb(rng, currU.y, s1, s2);
        return Vec2(u1, u2);
    }

    auto PerturbDirectionSample(const Path& currP, Random& rng, const Primitive* primitive, int i, TransportDirection transDir, Float s1, Float s2) -> boost::optional<Vec2>
    {
        assert((primitive->Type() & SurfaceInteractionType::S) == 0);
        #if INVERSEMAP_DEBUG_SIMPLIFY_INDEPENDENT
        return rng.Next2D();
        #else
        if ((primitive->Type() & SurfaceInteractionType::E) > 0)
        {
            // Perturb raster position
            const auto propRasterPos = PerturbRasterPos(currP, rng, s1, s2);
            if (!propRasterPos)
            {
                return boost::none;
            }
            return propRasterPos;
        }
        else
        {
            // Perturb direction sample
            const auto propDSample = PerturbDSample(currP, rng, i, transDir, s1, s2);
            if (!propDSample)
            {
                return boost::none;
            }
            return propDSample;
        }
        LM_UNREACHABLE();
        return boost::none;
        #endif
    }

}

#if INVERSEMAP_DEBUG_MLT_MANIFOLDWALK_STAT
namespace
{
    std::atomic<long long> manifoldWalkCount{0};
    std::atomic<long long> manifoldWalkSuccessCount{0};
}
#endif

// --------------------------------------------------------------------------------

auto MLTMutationStrategy::CheckMutatable_BidirFixed(const Path& currP) -> bool
{
    return true;
}

auto MLTMutationStrategy::CheckMutatable_Bidir(const Path& currP) -> bool
{
    return true;
}

auto MLTMutationStrategy::CheckMutatable_Lens(const Path& currP) -> bool
{
    const int n = (int)(currP.vertices.size());
    int iE = n - 1;
    int iL = iE - 1;
    while (iL >= 0 && currP.vertices[iL].type == SurfaceInteractionType::S) { iL--; }
    if (iL > 0 && currP.vertices[iL - 1].type == SurfaceInteractionType::S) { return false; }
    return true;
}

auto MLTMutationStrategy::CheckMutatable_Caustic(const Path& currP) -> bool
{
    const int n = (int)(currP.vertices.size());
    int iE = n - 1;
    int iL = iE - 1;
    if (n <= 2) { return false; }
    if (currP.vertices[iL].type == SurfaceInteractionType::S) { return false; }
    iL--;
    while (iL >= 0 && currP.vertices[iL].type == SurfaceInteractionType::S) { iL--; }
    return true;
}

auto MLTMutationStrategy::CheckMutatable_Multichain(const Path& currP) -> bool
{
    return true;
}

auto MLTMutationStrategy::CheckMutatable_ManifoldLens(const Path& currP) -> bool
{
    const auto type = currP.PathType();
    thread_local std::regex reg(R"x(^LS+[DG]S*E$)x");
    std::smatch match;
    if (!std::regex_match(type, match, reg)) { return false; }
    return true;
}

auto MLTMutationStrategy::CheckMutatable_ManifoldCaustic(const Path& currP) -> bool
{
    const auto type = currP.PathType();
    thread_local std::regex reg(R"x(^LS*[DG]S+E$)x");
    std::smatch match;
    if (!std::regex_match(type, match, reg)) { return false; }
    return true;
}

auto MLTMutationStrategy::CheckMutatable_Manifold(const Path& currP) -> bool
{
    const auto type = currP.PathType();
    thread_local std::regex reg(R"x(^L[DSG]*[DG][DSG]*E$)x");
    std::smatch match;
    if (!std::regex_match(type, match, reg)) { return false; }
    return true;
}

// --------------------------------------------------------------------------------

auto MLTMutationStrategy::Mutate_BidirFixed(const Scene* scene, Random& rng, const Path& currP) -> boost::optional<Prop>
{
    const int n = (int)(currP.vertices.size());

    // Implements bidirectional mutation within same path length
    // Some simplification
    //   - Mutation within the same path length

    // Choose # of path vertices to be deleted
    #if INVERSEMAP_DEBUG_SIMPLIFY_BIDIR_MUT_DELETE_ALL
    const int kd = n;
    #else
    TwoTailedGeometricDist removedPathVertexNumDist(2);
    removedPathVertexNumDist.Configure(1, 1, n);
    const int kd = removedPathVertexNumDist.Sample(rng.Next());
    #endif

    // Choose range of deleted vertices [dL,dM]
#if 0
    const int dL = Math::Clamp((int)(rng.Next() * (n - kd + 1)), 0, n - kd);
    const int dM = dL + kd - 1;
#else
    Distribution1D distD2;
    distD2.Clear();
    for (int l = 0; l <= n - kd; l++)
    {
        int m = l + kd - 1;
        if ((l > 0 && (currP.vertices[l - 1].type & SurfaceInteractionType::S) > 0) || (m < n - 1 && (currP.vertices[m + 1].type & SurfaceInteractionType::S) > 0))
        {
            distD2.Add(0_f);
        }
        else
        {
            distD2.Add(1_f);
        }
    }
    const int dL = distD2.Sample(rng.Next());
    const int dM = dL + kd - 1;
#endif

    // Choose # of vertices added from each endpoint
    #if INVERSEMAP_DEBUG_SIMPLIFY_BIDIR_MUT_PT
    const int aL = 0;
    const int aM = kd - aL;
    #else
    const int aL = Math::Clamp((int)(rng.Next() * (kd + 1)), 0, kd);
    const int aM = kd - aL;
    #endif

    // Sample subpaths
    Subpath subpathL;
    for (int s = 0; s < dL; s++)
    {
        subpathL.vertices.push_back(currP.vertices[s]);
    }
    if (subpathL.SampleSubpathFromEndpoint(scene, &rng, TransportDirection::LE, aL) != aL)
    {
        return boost::none;
    }

    Subpath subpathE;
    for (int t = n - 1; t > dM; t--)
    {
        subpathE.vertices.push_back(currP.vertices[t]);
    }
    if (subpathE.SampleSubpathFromEndpoint(scene, &rng, TransportDirection::EL, aM) != aM)
    {
        return boost::none;
    }

    // Create proposed path
    Prop prop;
    if (!prop.p.ConnectSubpaths(scene, subpathL, subpathE, (int)(subpathL.vertices.size()), (int)(subpathE.vertices.size())))
    {
        return boost::none;
    }

    // Reject paths with zero-contribution
    // Note that Q function is assumed to accept paths with positive contribution
    if (prop.p.EvaluateF(dL + aL).Black())
    {
        return boost::none;
    }

    prop.subspace.bidirfixed.kd = kd;
    prop.subspace.bidirfixed.dL = dL;
    return prop;
}

auto MLTMutationStrategy::Mutate_Bidir(const Scene* scene, Random& rng, const Path& currP, int maxPathVertices) -> boost::optional<Prop>
{
    const int currN = (int)(currP.vertices.size());

    // Choose # of path vertices of the proposed path
    TwoTailedGeometricDist propPathVertexNumDist(2);
    propPathVertexNumDist.Configure(currN, 2, maxPathVertices);
    const int propN = propPathVertexNumDist.Sample(rng.Next());

    // Choose # of path vertices to be deleted
    #if INVERSEMAP_DEBUG_SIMPLIFY_BIDIR_MUT_DELETE_ALL
    const int kd = currN;
    #else
    TwoTailedGeometricDist removedPathVertexNumDist(2);
    removedPathVertexNumDist.Configure(1, Math::Max(1, currN - propN), currN);
    const int kd = removedPathVertexNumDist.Sample(rng.Next());
    #endif

    // Number of vertices to be added
    const int ka = propN - currN + kd;

    // Choose range of deleted vertices [dL,dM]
#if 0
    const int dL = Math::Clamp((int)(rng.Next() * (currN - kd + 1)), 0, currN - kd);
    const int dM = dL + kd - 1;
#else
    Distribution1D distD2;
    distD2.Clear();
    for (int l = 0; l <= currN - kd; l++)
    {
        int m = l + kd - 1;
        if ((l > 0 && (currP.vertices[l - 1].type & SurfaceInteractionType::S) > 0) || (m < currN - 1 && (currP.vertices[m + 1].type & SurfaceInteractionType::S) > 0))
        {
            distD2.Add(0_f);
        }
        else
        {
            distD2.Add(1_f);
        }
    }
    if (distD2.Sum() == 0_f) { return boost::none; }
    distD2.Normalize();
    const int dL = distD2.Sample(rng.Next());
    const int dM = dL + kd - 1;
#endif

    // Choose # of vertices added from each endpoint
    #if INVERSEMAP_DEBUG_SIMPLIFY_BIDIR_MUT_PT
    const int aL = 0;
    const int aM = ka;
    #else
    const int aL = Math::Clamp((int)(rng.Next() * (ka + 1)), 0, ka);
    const int aM = ka - aL;
    #endif

    // Sample subpaths
    Subpath subpathL;
    subpathL.vertices.clear();
    for (int s = 0; s < dL; s++)
    {
        subpathL.vertices.push_back(currP.vertices[s]);
    }
    if (subpathL.SampleSubpathFromEndpoint(scene, &rng, TransportDirection::LE, aL) != aL)
    {
        return boost::none;
    }

    Subpath subpathE;
    subpathE.vertices.clear();
    for (int t = currN - 1; t > dM; t--)
    {
        subpathE.vertices.push_back(currP.vertices[t]);
    }
    if (subpathE.SampleSubpathFromEndpoint(scene, &rng, TransportDirection::EL, aM) != aM)
    {
        return boost::none;
    }

    // Create proposed path
    Prop prop;
    if (!prop.p.ConnectSubpaths(scene, subpathL, subpathE, (int)(subpathL.vertices.size()), (int)(subpathE.vertices.size())))
    {
        return boost::none;
    }

    // Reject paths with zero-contribution
    // Note that Q function is assumed to accept paths with positive contribution
    if (prop.p.EvaluateF(dL + aL).Black())
    {
        return boost::none;
    }

    prop.subspace.bidir.kd = kd;
    prop.subspace.bidir.ka = ka;
    prop.subspace.bidir.dL = dL;
    return prop;
}

auto MLTMutationStrategy::Mutate_Lens(const Scene* scene, Random& rng, const Path& currP, Float s1, Float s2) -> boost::optional<Prop>
{
    const int n = (int)(currP.vertices.size());

    // Check if the strategy can mutate the current path
    // Acceptable path type: D/L/empty D/L S* E
    {
        int iE = n - 1;
        int iL = iE - 1;
        //iL--;
        while (iL >= 0 && currP.vertices[iL].type == SurfaceInteractionType::S) { iL--; }
        if (iL > 0 && currP.vertices[iL - 1].type == SurfaceInteractionType::S) { return boost::none;  }
    }

    // Eye subpath
    const auto subpathE = [&]() -> boost::optional<Subpath>
    {
        Subpath subpathE;
        subpathE.vertices.push_back(currP.vertices[n - 1]);

        // Trace subpath
        bool failed = false;
        SubpathSampler::TraceSubpathFromEndpointWithSampler(scene, &subpathE.vertices[0], nullptr, 1, n, TransportDirection::EL,
            [&](int numVertices, const Primitive* primitive, SubpathSampler::SampleUsage usage, int index) -> Float
            {
                if (primitive && usage == SubpathSampler::SampleUsage::Direction && (primitive->Type() & SurfaceInteractionType::S) == 0)
                {
                    const auto propU = PerturbDirectionSample(currP, rng, primitive, numVertices - 2, TransportDirection::EL, s1, s2);
                    if (!propU)
                    {
                        failed = true;
                        return 0_f;
                    }
                    return (*propU)[index];
                }
                return rng.Next();
            },
            [&](int numVertices, const Vec2& /*rasterPos*/, const SubpathSampler::SubpathSampler::PathVertex& pv, const SubpathSampler::SubpathSampler::PathVertex& v, SPD& throughput) -> bool
            {
                assert(numVertices > 1);
                subpathE.vertices.emplace_back(v);

                // Reject if the corresponding vertex in the current path is not S
                {
                    const auto propVT = (v.primitive->Type() & SurfaceInteractionType::S) > 0;
                    const auto currVT = (currP.vertices[n - numVertices].primitive->Type() & SurfaceInteractionType::S) > 0;
                    if (propVT != currVT)
                    {
                        failed = true;
                        return false;
                    }
                }
                                    
                // Continue to trace if intersected vertex is S
                if ((v.primitive->Type() & SurfaceInteractionType::S) > 0)
                {
                    return true;
                }

                assert((v.primitive->Type() & SurfaceInteractionType::D) > 0 || (v.primitive->Type() & SurfaceInteractionType::G) > 0);
                return false;
            }
        );
        if (failed)
        {
            return boost::none;
        }

        return subpathE;
    }();
    if (!subpathE)
    {
        return boost::none;
    }

    // Sampling is failed if the last vertex is S or E or point at infinity
    {
        const auto& vE = subpathE->vertices.back();
        if (vE.geom.infinite || (vE.primitive->Type() & SurfaceInteractionType::E) > 0 || (vE.primitive->Type() & SurfaceInteractionType::S) > 0)
        {
            return boost::none;
        }
    }

    // Number of vertices in each subpath
    const int nE = (int)(subpathE->vertices.size());
    const int nL = n - nE;

    // Light subpath
    const auto subpathL = [&]() -> Subpath
    {
        Subpath subpathL;
        for (int s = 0; s < nL; s++)
        {
            subpathL.vertices.push_back(currP.vertices[s]);
        }
        return subpathL;
    }();

    // Connect subpaths and create a proposed path
    Prop prop;
    if (!prop.p.ConnectSubpaths(scene, subpathL, *subpathE, nL, nE))
    {
        return boost::none;
    }
                            
    // Reject paths with zero-contribution (reject e.g., S + DS paths)
    if (prop.p.EvaluateF(nL).Black())
    {
        return boost::none;
    }

    return prop;
}

auto MLTMutationStrategy::Mutate_Caustic(const Scene* scene, Random& rng, const Path& currP, Float s1, Float s2) -> boost::optional<Prop>
{
    const int n = (int)(currP.vertices.size());

    // Check if the strategy can mutate the current path
    // Acceptable path type: D/L S* D/G E
    const auto iL = [&]() -> boost::optional<int>
    {
        int iE = n - 1;
        int iL = iE - 1;

        // Cannot support LE paths
        if (n <= 2) { return boost::none; }

        // Reject if the vertex next to E is not S
        if (currP.vertices[iL].type == SurfaceInteractionType::S) { return boost::none; }

        // Find first non-S vertex
        iL--;
        while (iL >= 0 && currP.vertices[iL].type == SurfaceInteractionType::S) { iL--; }

        return iL;
    }();
    if (!iL)
    {
        return boost::none;
    }

    // Light subpath
    const auto subpathL = [&]() -> boost::optional<Subpath>
    {
        Subpath subpathL;
        for (int s = 0; s <= *iL; s++) { subpathL.vertices.push_back(currP.vertices[s]); }
        bool failed = false;

        // Trace subpath
        SubpathSampler::TraceSubpathFromEndpointWithSampler(scene, &subpathL.vertices[*iL], *iL > 0 ? &subpathL.vertices[*iL - 1] : nullptr, *iL + 1, n, TransportDirection::LE,
            [&](int numVertices, const Primitive* primitive, SubpathSampler::SampleUsage usage, int index) -> Float
        {
            if (primitive && usage == SubpathSampler::SampleUsage::Direction && (primitive->Type() & SurfaceInteractionType::S) == 0)
            {
                assert(*iL == numVertices - 2);
                const auto propU = PerturbDirectionSample(currP, rng, primitive, numVertices - 2, TransportDirection::LE, s1, s2);
                if (!propU)
                {
                    failed = true;
                    return 0_f;
                }
                return (*propU)[index];
            }
            return rng.Next();
        },
            [&](int numVertices, const Vec2& /*rasterPos*/, const SubpathSampler::SubpathSampler::PathVertex& pv, const SubpathSampler::SubpathSampler::PathVertex& v, SPD& throughput) -> bool
        {
            subpathL.vertices.push_back(v);

            // Reject if the corresponding vertex in the current path is not S
            {
                const auto propVT = (v.primitive->Type() & SurfaceInteractionType::S) > 0;
                const auto currVT = (currP.vertices[numVertices - 1].primitive->Type() & SurfaceInteractionType::S) > 0;
                if (propVT != currVT)
                {
                    failed = true;
                    return false;
                }
            }

            // Continue to trace if intersected vertex is S
            if ((v.primitive->Type() & SurfaceInteractionType::S) > 0)
            {
                return true;
            }

            assert((v.primitive->Type() & SurfaceInteractionType::D) > 0 || (v.primitive->Type() & SurfaceInteractionType::G) > 0);
            return false;
        }
        );
        if (failed)
        {
            return boost::none;
        }

        return subpathL;
    }();
    if (!subpathL)
    {
        return boost::none;
    }

    // Sampling is failed if the last vertex is S or E or point at infinity
    {
        if (n != (int)subpathL->vertices.size() + 1)
        {
            return boost::none;
        }
        const auto& vL = subpathL->vertices.back();
        if (vL.geom.infinite || (vL.primitive->Type() & SurfaceInteractionType::S) > 0)
        {
            return boost::none;
        }
    }

    // Eye subpath
    Subpath subpathE;
    subpathE.vertices.push_back(currP.vertices[n - 1]);

    // Connect subpaths and create a proposed path
    Prop prop;
    if (!prop.p.ConnectSubpaths(scene, *subpathL, subpathE, (int)(subpathL->vertices.size()), 1))
    {
        return boost::none;
    }

    // Reject paths with zero-contribution (reject e.g., S + DS paths)
    if (prop.p.EvaluateF((int)(subpathL->vertices.size())).Black())
    {
        return boost::none;
    }

    return prop;
}

auto MLTMutationStrategy::Mutate_Multichain(const Scene* scene, Random& rng, const Path& currP, Float s1, Float s2) -> boost::optional<Prop>
{
    const int n = (int)(currP.vertices.size());
                 
    // Eye subpath
    const auto subpathE = [&]() -> boost::optional<Subpath>
    {
        Subpath subpathE;
        subpathE.vertices.push_back(currP.vertices[n - 1]);
                                
        // Trace subpath
        bool failed = false;
        SubpathSampler::TraceSubpathFromEndpointWithSampler(scene, &subpathE.vertices[0], nullptr, 1, n, TransportDirection::EL,
            [&](int numVertices, const Primitive* primitive, SubpathSampler::SampleUsage usage, int index) -> Float
            {
                // Perturb sample used for sampling direction
                if (primitive && usage == SubpathSampler::SampleUsage::Direction && (primitive->Type() & SurfaceInteractionType::S) == 0)
                {
                    const auto propU = PerturbDirectionSample(currP, rng, primitive, numVertices - 2, TransportDirection::EL, s1, s2);
                    if (!propU)
                    {
                        failed = true;
                        return 0_f;
                    }
                    return (*propU)[index];
                }
                return rng.Next();
            },
            [&](int numVertices, const Vec2& /*rasterPos*/, const SubpathSampler::SubpathSampler::PathVertex& pv, const SubpathSampler::SubpathSampler::PathVertex& v, SPD& throughput) -> bool
            {
                assert(numVertices > 1);
                subpathE.vertices.emplace_back(v);

                // Reject if the corresponding vertex in the current path is not S
                {
                    const auto propVT = (v.primitive->Type() & SurfaceInteractionType::S) > 0;
                    const auto currVT = (currP.vertices[n - numVertices].primitive->Type() & SurfaceInteractionType::S) > 0;
                    if (propVT != currVT)
                    {
                        failed = true;
                        return false;
                    }
                }
                                    
                // Continue to trace if intersected vertex is S
                if ((v.primitive->Type() & SurfaceInteractionType::S) > 0)
                {
                    return true;
                }

                assert((v.primitive->Type() & SurfaceInteractionType::D) > 0 || (v.primitive->Type() & SurfaceInteractionType::G) > 0);

                // Stop if current vertex is the last one
                if (n - numVertices == 0)
                {
                    return false;
                }
                    
                assert(n - numVertices > 0);

                // Stop if corresponding next vertex is not S
                if ((currP.vertices[n - numVertices - 1].primitive->Type() & SurfaceInteractionType::S) == 0)
                {
                    return false;
                }

                // Otherwise continue
                return true;
            }
        );
        if (failed)
        {
            return boost::none;
        }

        return subpathE;
    }();
    if (!subpathE)
    {
        return boost::none;
    }
                            
    // Sampling is failed if the last vertex is S or E or point at infinity
    {
        const auto& vE = subpathE->vertices.back();
        if (vE.geom.infinite || (vE.primitive->Type() & SurfaceInteractionType::E) > 0 || (vE.primitive->Type() & SurfaceInteractionType::S) > 0)
        {
            return boost::none;
        }
    }

    // Number of vertices in each subpath
    const int nE = (int)(subpathE->vertices.size());
    const int nL = n - nE;

    // Light subpath
    const auto subpathL = [&]() -> Subpath
    {
        Subpath subpathL;
        for (int s = 0; s < nL; s++)
        {
            subpathL.vertices.push_back(currP.vertices[s]);
        }
        return subpathL;
    }();

    // Connect subpaths and create a proposed path
    Prop prop;
    if (!prop.p.ConnectSubpaths(scene, subpathL, *subpathE, nL, nE))
    {
        return boost::none;
    }

    // Reject paths with zero-contribution (reject e.g., S + DS paths)
    if (prop.p.EvaluateF(nL).Black())
    {
        return boost::none;
    }

    return prop;
}

auto MLTMutationStrategy::Mutate_ManifoldLens(const Scene* scene, Random& rng, const Path& currP, Float s1, Float s2) -> boost::optional<Prop>
{
    // L | S* | DS+E
    const int n = (int)(currP.vertices.size());

    // --------------------------------------------------------------------------------

    #pragma region Check if current path can be mutated with the strategy
    if (!CheckMutatable_ManifoldLens(currP))
    {
        return boost::none;
    }
    #pragma endregion

    // --------------------------------------------------------------------------------
    
    #if INVERSEMAP_MLT_DEBUG_IO
    //{
    //    LM_LOG_DEBUG("manifoldlens_current_path");
    //    DebugIO::Wait();
    //    std::vector<double> vs;
    //    for (const auto& v : currP.vertices) for (int i = 0; i < 3; i++) vs.push_back(v.geom.p[i]);
    //    std::stringstream ss;
    //    {
    //        cereal::JSONOutputArchive oa(ss);
    //        oa(vs);
    //    }
    //    DebugIO::Output("manifoldlens_current_path", ss.str());
    //}
    #endif

    // --------------------------------------------------------------------------------

    #pragma region Perturb eye subpath
    const auto subpathE = [&]() -> boost::optional<Subpath>
    {
        Subpath subpathE;
        subpathE.vertices.push_back(currP.vertices[n - 1]);

        // Trace subpath
        bool failed = false;
        SubpathSampler::TraceSubpathFromEndpointWithSampler(scene, &subpathE.vertices[0], nullptr, 1, n, TransportDirection::EL,
            [&](int numVertices, const Primitive* primitive, SubpathSampler::SampleUsage usage, int index) -> Float
            {
                if (primitive && usage == SubpathSampler::SampleUsage::Direction && (primitive->Type() & SurfaceInteractionType::S) == 0)
                {
                    const auto propU = PerturbDirectionSample(currP, rng, primitive, numVertices - 2, TransportDirection::EL, s1, s2);
                    if (!propU)
                    {
                        failed = true;
                        return 0_f;
                    }
                    return (*propU)[index];
                }
                return rng.Next();
            },
            [&](int numVertices, const Vec2& /*rasterPos*/, const SubpathSampler::SubpathSampler::PathVertex& pv, const SubpathSampler::SubpathSampler::PathVertex& v, SPD& throughput) -> bool
            {
                assert(numVertices > 1);
                subpathE.vertices.emplace_back(v);

                // Reject if the corresponding vertex in the current path is not S
                {
                    const auto propVT = (v.primitive->Type() & SurfaceInteractionType::S) > 0;
                    const auto currVT = (currP.vertices[n - numVertices].primitive->Type() & SurfaceInteractionType::S) > 0;
                    if (propVT != currVT)
                    {
                        failed = true;
                        return false;
                    }
                }
                                    
                // Continue to trace if intersected vertex is S
                if ((v.primitive->Type() & SurfaceInteractionType::S) > 0)
                {
                    return true;
                }

                assert((v.primitive->Type() & SurfaceInteractionType::D) > 0 || (v.primitive->Type() & SurfaceInteractionType::G) > 0);
                return false;
            });
        if (failed)
        {
            return boost::none;
        }
        {
            // Sampling is failed if the last vertex is S or E or point at infinity
            const auto& vE = subpathE.vertices.back();
            if (vE.geom.infinite || (vE.primitive->Type() & SurfaceInteractionType::E) > 0 || (vE.primitive->Type() & SurfaceInteractionType::S) > 0)
            {
                return boost::none;
            }
        }
        return subpathE;
    }();
    if (!subpathE)
    {
        return boost::none;
    }
    #pragma endregion

    // --------------------------------------------------------------------------------

    #if INVERSEMAP_MLT_DEBUG_IO
    //{
    //    LM_LOG_DEBUG("manifoldlens_perturbed_subpath");
    //    DebugIO::Wait();
    //    std::vector<double> vs;
    //    for (const auto& v : subpathE->vertices) for (int i = 0; i < 3; i++) vs.push_back(v.geom.p[i]);
    //    std::stringstream ss;
    //    {
    //        cereal::JSONOutputArchive oa(ss);
    //        oa(vs);
    //    }
    //    DebugIO::Output("manifoldlens_perturbed_subpath", ss.str());
    //}
    #endif

    // --------------------------------------------------------------------------------

    #pragma region Connect light subapth
    const auto subpathL = [&]() -> boost::optional<Subpath>
    {
        // Original light subpath (LS*D)
        Subpath subpathL_Orig;
        const int nE = (int)(subpathE->vertices.size());
        const int nL = n - nE;
        for (int s = 0; s < nL + 1; s++)
        {
            subpathL_Orig.vertices.push_back(currP.vertices[s]);
        }

        // Manifold walk
        #if INVERSEMAP_DEBUG_MLT_MANIFOLDWALK_STAT
        manifoldWalkCount++;
        #endif
        const auto connPath = ManifoldUtils::WalkManifold(scene, subpathL_Orig, subpathE->vertices.back().geom.p);
        if (!connPath)
        {
            return boost::none;
        }
        const auto connPathInv = ManifoldUtils::WalkManifold(scene, *connPath, subpathL_Orig.vertices.back().geom.p);
        if (!connPathInv)
        {
            return boost::none;
        }
        #if INVERSEMAP_DEBUG_MLT_MANIFOLDWALK_STAT
        manifoldWalkSuccessCount++;
        #endif

        return *connPath;
    }();
    if (!subpathL)
    {
        return boost::none;
    }
    #pragma endregion

    // --------------------------------------------------------------------------------

    Prop prop;
    {
        const int nE = (int)(subpathE->vertices.size());
        const int nL = n - nE;
        for (int s = 0; s < nL; s++)      { prop.p.vertices.push_back(subpathL->vertices[s]); }
        for (int t = nE - 1; t >= 0; t--) { prop.p.vertices.push_back(subpathE->vertices[t]); }
        if (prop.p.EvaluateF(0).Black())
        {
            // Reject paths with zero-contribution
            return boost::none;
        }
    }

    // --------------------------------------------------------------------------------
    
    #if INVERSEMAP_MLT_DEBUG_IO
    //{
    //    LM_LOG_DEBUG("manifoldlens_proposed_path");
    //    DebugIO::Wait();
    //    std::vector<double> vs;
    //    for (const auto& v : prop.p.vertices) for (int i = 0; i < 3; i++) vs.push_back(v.geom.p[i]);
    //    std::stringstream ss;
    //    {
    //        cereal::JSONOutputArchive oa(ss);
    //        oa(vs);
    //    }
    //    DebugIO::Output("manifoldlens_proposed_path", ss.str());
    //}
    #endif

    // --------------------------------------------------------------------------------
    return prop;
}

auto MLTMutationStrategy::Mutate_ManifoldCaustic(const Scene* scene, Random& rng, const Path& currP, Float s1, Float s2) -> boost::optional<Prop>
{
    // LS+D | S* | E
    const int n = (int)(currP.vertices.size());

    // --------------------------------------------------------------------------------

    #pragma region Check if current path can be mutated with the strategy
    if (!CheckMutatable_ManifoldCaustic(currP))
    {
        return boost::none;
    }
    #pragma endregion

    // --------------------------------------------------------------------------------

    #if INVERSEMAP_MLT_DEBUG_IO
    //{
    //    LM_LOG_DEBUG("manifoldlens_current_path");
    //    DebugIO::Wait();
    //    std::vector<double> vs;
    //    for (const auto& v : currP.vertices) for (int i = 0; i < 3; i++) vs.push_back(v.geom.p[i]);
    //    std::stringstream ss;
    //    {
    //        cereal::JSONOutputArchive oa(ss);
    //        oa(vs);
    //    }
    //    DebugIO::Output("manifoldlens_current_path", ss.str());
    //}
    #endif

    // --------------------------------------------------------------------------------

    #pragma region Perturb light subpath
    const auto subpathL = [&]() -> boost::optional<Subpath>
    {
        Subpath subpathL;
        subpathL.vertices.push_back(currP.vertices[0]);

        // Trace subpath
        bool failed = false;
        SubpathSampler::TraceSubpathFromEndpointWithSampler(scene, &subpathL.vertices[0], nullptr, 1, n, TransportDirection::LE,
            [&](int numVertices, const Primitive* primitive, SubpathSampler::SampleUsage usage, int index) -> Float
            {
                if (primitive && usage == SubpathSampler::SampleUsage::Direction && (primitive->Type() & SurfaceInteractionType::S) == 0)
                {
                    const auto propU = PerturbDirectionSample(currP, rng, primitive, numVertices - 2, TransportDirection::LE, s1, s2);
                    if (!propU)
                    {
                        failed = true;
                        return 0_f;
                    }
                    return (*propU)[index];
                }
                return rng.Next();
            },
            [&](int numVertices, const Vec2& /*rasterPos*/, const SubpathSampler::SubpathSampler::PathVertex& pv, const SubpathSampler::SubpathSampler::PathVertex& v, SPD& throughput) -> bool
            {
                assert(numVertices > 1);
                subpathL.vertices.emplace_back(v);

                // Reject if the corresponding vertex in the current path is not S
                {
                    const auto propVT = (v.primitive->Type() & SurfaceInteractionType::S) > 0;
                    const auto currVT = (currP.vertices[numVertices - 1].primitive->Type() & SurfaceInteractionType::S) > 0;
                    if (propVT != currVT)
                    {
                        failed = true;
                        return false;
                    }
                }

                // Continue to trace if intersected vertex is S
                if ((v.primitive->Type() & SurfaceInteractionType::S) > 0)
                {
                    return true;
                }

                assert((v.primitive->Type() & SurfaceInteractionType::D) > 0 || (v.primitive->Type() & SurfaceInteractionType::G) > 0);
                return false;
            });
        if (failed)
        {
            return boost::none;
        }
        {
            // Sampling is failed if the last vertex is S or L or point at infinity
            const auto& vL = subpathL.vertices.back();
            if (vL.geom.infinite || (vL.primitive->Type() & SurfaceInteractionType::L) > 0 || (vL.primitive->Type() & SurfaceInteractionType::S) > 0)
            {
                return boost::none;
            }
        }
        return subpathL;
    }();
    if (!subpathL)
    {
        return boost::none;
    }
    #pragma endregion

    // --------------------------------------------------------------------------------

    #if INVERSEMAP_MLT_DEBUG_IO
    //{
    //    LM_LOG_DEBUG("manifoldlens_perturbed_subpath");
    //    DebugIO::Wait();
    //    std::vector<double> vs;
    //    for (const auto& v : subpathL->vertices) for (int i = 0; i < 3; i++) vs.push_back(v.geom.p[i]);
    //    std::stringstream ss;
    //    {
    //        cereal::JSONOutputArchive oa(ss);
    //        oa(vs);
    //    }
    //    DebugIO::Output("manifoldlens_perturbed_subpath", ss.str());
    //}
    #endif

    // --------------------------------------------------------------------------------

    #pragma region Connect eye subapth
    const auto subpathE = [&]() -> boost::optional<Subpath>
    {
        // Original eye subpath (ES*D)
        Subpath subpathE_Orig;
        const int nL = (int)(subpathL->vertices.size());
        const int nE = n - nL;
        for (int t = 0; t < nE + 1; t++)
        {
            subpathE_Orig.vertices.push_back(currP.vertices[n - 1 - t]);
        }
        
        // Manifold walk
        #if INVERSEMAP_DEBUG_MLT_MANIFOLDWALK_STAT
        manifoldWalkCount++;
        #endif
        const auto connPath = ManifoldUtils::WalkManifold(scene, subpathE_Orig, subpathL->vertices.back().geom.p);
        if (!connPath)
        {
            return boost::none;
        }
        const auto connPathInv = ManifoldUtils::WalkManifold(scene, *connPath, subpathE_Orig.vertices.back().geom.p);
        if (!connPathInv)
        {
            return boost::none;
        }
        #if INVERSEMAP_DEBUG_MLT_MANIFOLDWALK_STAT
        manifoldWalkSuccessCount++;
        #endif

        return *connPath;
    }();
    if (!subpathE)
    {
        return boost::none;
    }
    #pragma endregion

    // --------------------------------------------------------------------------------

    Prop prop;
    {
        const int nE = (int)(subpathE->vertices.size());
        const int nL = n - nE;
        for (int s = 0; s < nL; s++) { prop.p.vertices.push_back(subpathL->vertices[s]); }
        for (int t = nE - 1; t >= 0; t--) { prop.p.vertices.push_back(subpathE->vertices[t]); }
        if (prop.p.EvaluateF(0).Black())
        {
            // Reject paths with zero-contribution
            return boost::none;
        }
    }

    // --------------------------------------------------------------------------------
    
    #if INVERSEMAP_MLT_DEBUG_IO
    //{
    //    LM_LOG_DEBUG("manifoldlens_proposed_path");
    //    DebugIO::Wait();
    //    std::vector<double> vs;
    //    for (const auto& v : prop.p.vertices) for (int i = 0; i < 3; i++) vs.push_back(v.geom.p[i]);
    //    std::stringstream ss;
    //    {
    //        cereal::JSONOutputArchive oa(ss);
    //        oa(vs);
    //    }
    //    DebugIO::Output("manifoldlens_proposed_path", ss.str());
    //}
    #endif

    // --------------------------------------------------------------------------------
    return prop;
}

auto MLTMutationStrategy::Mutate_Manifold(const Scene* scene, Random& rng, const Path& currP, Float s1, Float s2) -> boost::optional<Prop>
{
    const int n = (int)(currP.vertices.size());

    // --------------------------------------------------------------------------------

    #pragma region Check if current path can be mutated with the strategy
    if (!CheckMutatable_Manifold(currP))
    {
        return boost::none;
    }
    #pragma endregion

    // --------------------------------------------------------------------------------

    #if INVERSEMAP_MLT_DEBUG_IO
    {
        LM_LOG_DEBUG("manifoldlens_current_path");
        DebugIO::Wait();
        std::vector<double> vs;
        for (const auto& v : currP.vertices) for (int i = 0; i < 3; i++) vs.push_back(v.geom.p[i]);
        std::stringstream ss;
        {
            cereal::JSONOutputArchive oa(ss);
            oa(vs);
        }
        DebugIO::Output("manifoldlens_current_path", ss.str());
    }
    #endif

    // --------------------------------------------------------------------------------

    #pragma region Select subspace
    struct ManifoldSubspace
    {
        int ia;
        int ib;
        int ic;
    };
    const auto subspace = [&]() -> boost::optional<ManifoldSubspace>
    {
        // Indices of non-S vertices
        std::vector<int> nonSIndices;
        {
            nonSIndices.clear();
            for (size_t i = 0; i < currP.vertices.size(); i++)
            {
                const auto& v = currP.vertices[i];
                if ((v.primitive->Type() & SurfaceInteractionType::S) == 0)
                {
                    nonSIndices.push_back((int)i);
                }
            }
        }
        // Requires at least 3 non-S vertices
        if (nonSIndices.size() < 3)
        {
            return boost::none;
        }
        
        // ia
        const auto ia = [&]() -> int
        {
            const int i = Math::Clamp((int)(rng.Next() * (nonSIndices.size() - 2)), 0, (int)(nonSIndices.size()) - 3);
            return nonSIndices[i];
        }();

        // ib, ic
        const auto NearestNonSIndexFrom = [&](int ii) -> int
        {
            int i = ii + 1;
            while ((currP.vertices[i].primitive->Type() & SurfaceInteractionType::S) != 0) { i++; }
            return i;
        };
        const auto ib = NearestNonSIndexFrom(ia);
        const auto ic = NearestNonSIndexFrom(ib);

        return ManifoldSubspace{ ia, ib, ic };
    }();
    if (!subspace)
    {
        return boost::none;
    }
    #pragma endregion

    // --------------------------------------------------------------------------------

    #pragma region Perturb light subpath
    Subpath subpathL;
    subpathL.vertices.clear();
    if (![&]() -> bool
    {
        for (int i = 0; i <= subspace->ia; i++) { subpathL.vertices.push_back(currP.vertices[i]); }

        // Trace subpath
        bool failed = false;
        SubpathSampler::TraceSubpathFromEndpointWithSampler(scene, &subpathL.vertices.back(), nullptr, (int)subpathL.vertices.size(), n, TransportDirection::LE,
            [&](int numVertices, const Primitive* primitive, SubpathSampler::SampleUsage usage, int index) -> Float
            {
                if (primitive && usage == SubpathSampler::SampleUsage::Direction && (primitive->Type() & SurfaceInteractionType::S) == 0)
                {
                    const auto propU = PerturbDirectionSample(currP, rng, primitive, numVertices - 2, TransportDirection::LE, s1, s2);
                    if (!propU)
                    {
                        failed = true;
                        return 0_f;
                    }
                    return (*propU)[index];
                }
                return rng.Next();
            },
            [&](int numVertices, const Vec2& /*rasterPos*/, const SubpathSampler::SubpathSampler::PathVertex& pv, const SubpathSampler::SubpathSampler::PathVertex& v, SPD& throughput) -> bool
            {
                assert(numVertices > 1);
                subpathL.vertices.emplace_back(v);

                // Reject if the corresponding vertex in the current path is not S
                {
                    const auto propVT = (v.primitive->Type() & SurfaceInteractionType::S) > 0;
                    const auto currVT = (currP.vertices[numVertices - 1].primitive->Type() & SurfaceInteractionType::S) > 0;
                    if (propVT != currVT)
                    {
                        failed = true;
                        return false;
                    }
                }

                // Continue to trace if intersected vertex is S
                if ((v.primitive->Type() & SurfaceInteractionType::S) > 0)
                {
                    return true;
                }

                assert((v.primitive->Type() & SurfaceInteractionType::D) > 0 || (v.primitive->Type() & SurfaceInteractionType::G) > 0);
                return false;
            });
        if (failed)
        {
            return false;
        }
        {
            // Sampling is failed if the last vertex is S or point at infinity, or no change in number of vertices
            const auto& vL = subpathL.vertices.back();
            if (vL.geom.infinite || (vL.primitive->Type() & SurfaceInteractionType::S) > 0 || (int)subpathL.vertices.size() != subspace->ib + 1)
            {
                return false;
            }
        }

        return true;
    }())
    {
        return boost::none;
    }
    #pragma endregion

    // --------------------------------------------------------------------------------

    #if INVERSEMAP_MLT_DEBUG_IO
    {
        LM_LOG_DEBUG("manifoldlens_perturbed_subpath");
        DebugIO::Wait();
        std::vector<double> vs;
        for (const auto& v : subpathL->vertices) for (int i = 0; i < 3; i++) vs.push_back(v.geom.p[i]);
        std::stringstream ss;
        {
            cereal::JSONOutputArchive oa(ss);
            oa(vs);
        }
        DebugIO::Output("manifoldlens_perturbed_subpath", ss.str());
    }
    #endif

    // --------------------------------------------------------------------------------

    #pragma region Connect eye subapth
    Subpath subpathE;
    subpathE.vertices.clear();
    if (![&]() -> bool
    {
        // Partial subpath [ib,ic]
        Subpath subpathE_Orig;
        subpathE_Orig.vertices.clear();
        for (int i = subspace->ic; i >= subspace->ib; i--) { subpathE_Orig.vertices.push_back(currP.vertices[i]); }
        
        // Conenct
        Subpath connPath;
        connPath.vertices.clear();
        if (![&]() -> bool
        {
            if (subspace->ib + 1 == subspace->ic)
            {
                // Path connection
                const auto& vL = subpathL.vertices.back();
                const auto& vE = currP.vertices[subspace->ic];
                if (vL.geom.infinite || vE.geom.infinite)  { return false; }
                if (!scene->Visible(vL.geom.p, vE.geom.p)) { return false; }
                connPath.vertices.push_back(vE);
                connPath.vertices.push_back(vL);
                return true;
            }
            else
            {
                // Manifold walk
                #if INVERSEMAP_DEBUG_MLT_MANIFOLDWALK_STAT
                manifoldWalkCount++;
                #endif
                if (!ManifoldUtils::WalkManifold(scene, subpathE_Orig, subpathL.vertices.back().geom.p, connPath)) { return false; }
                Subpath connPathInv;
                connPathInv.vertices.clear();
                if (!ManifoldUtils::WalkManifold(scene, connPath, subpathE_Orig.vertices.back().geom.p, connPathInv)) { return false; }
                #if INVERSEMAP_DEBUG_MLT_MANIFOLDWALK_STAT
                manifoldWalkSuccessCount++;
                #endif
                return true;
            }
            LM_UNREACHABLE();
            return false;
        }())
        {
            return false;
        }

        // Connected eye subpath
        for (int i = n - 1; i > subspace->ic; i--) { subpathE.vertices.push_back(currP.vertices[i]); }
        for (const auto& v : connPath.vertices) { subpathE.vertices.push_back(v); }

        return true;
    }())
    {
        return boost::none;
    }
    #pragma endregion

    // --------------------------------------------------------------------------------

    Prop prop;
    {
        prop.subspace.manifold.ia = subspace->ia;
        prop.subspace.manifold.ib = subspace->ib;
        prop.subspace.manifold.ic = subspace->ic;
        const int nE = (int)(subpathE.vertices.size());
        const int nL = n - nE;
        for (int s = 0; s < nL; s++) { prop.p.vertices.push_back(subpathL.vertices[s]); }
        for (int t = nE - 1; t >= 0; t--) { prop.p.vertices.push_back(subpathE.vertices[t]); }
        if (prop.p.EvaluateF(0).Black())
        {
            // Reject paths with zero-contribution
            return boost::none;
        }
    }

    // --------------------------------------------------------------------------------
    
    #if INVERSEMAP_MLT_DEBUG_IO
    {
        LM_LOG_DEBUG("manifoldlens_proposed_path");
        DebugIO::Wait();
        std::vector<double> vs;
        for (const auto& v : prop.p.vertices) for (int i = 0; i < 3; i++) vs.push_back(v.geom.p[i]);
        std::stringstream ss;
        {
            cereal::JSONOutputArchive oa(ss);
            oa(vs);
        }
        DebugIO::Output("manifoldlens_proposed_path", ss.str());
    }
    #endif

    // --------------------------------------------------------------------------------
    //LM_LOG_INFO(boost::str(boost::format("Selected subspace (%d,%d,%d)") % subspace->ia % subspace->ib % subspace->ic));
    return prop;
}

// --------------------------------------------------------------------------------

auto MLTMutationStrategy::Q_BidirFixed(const Scene* scene, const Path& x, const Path& y, const Subspace& subspace) -> Float
{
    const int xN = (int)(x.vertices.size());

#if 0
    const auto pD2 = 1_f / (Float)(xN - subspace.bidirfixed.kd + 1);
#else
    Distribution1D distD2;
    distD2.Clear();
    for (int l = 0; l <= xN - subspace.bidirfixed.kd; l++)
    {
        int m = l + subspace.bidirfixed.kd - 1;
        if ((l > 0 && (x.vertices[l - 1].type & SurfaceInteractionType::S) > 0) || (m < xN - 1 && (x.vertices[m + 1].type & SurfaceInteractionType::S) > 0))
        {
            distD2.Add(0_f);
        }
        else
        {
            distD2.Add(1_f);
        }
    }
    const auto pD2 = distD2.EvaluatePDF(subspace.bidirfixed.dL);
#endif

    Float sum = 0_f;
    #if INVERSEMAP_DEBUG_SIMPLIFY_BIDIR_MUT_PT
    for (int i = 0; i <= 0; i++)
    #else
    for (int i = 0; i <= subspace.bidirfixed.kd; i++)
    #endif
    {
        const auto f = InversemapUtils::ScalarContrb(y.EvaluateF(subspace.bidirfixed.dL + i));
        if (f == 0_f)
        {
            continue;
        }
        const auto p = y.EvaluatePathPDF(scene, subspace.bidirfixed.dL + i);
        assert(p.v > 0_f);
        const auto C = f / p.v;
        sum += 1_f / C;
    }
    return pD2 * sum;
}

auto MLTMutationStrategy::Q_Bidir(const Scene* scene, const Path& x, const Path& y, const Subspace& subspace, int maxPathVertices) -> Float
{
    const int xN = (int)(x.vertices.size());
    const int yN = (int)(y.vertices.size());

    // pA1
    TwoTailedGeometricDist propPathVertexNumDist(2);
    propPathVertexNumDist.Configure(xN, 2, maxPathVertices);
    const auto pA1 = propPathVertexNumDist.EvaluatePDF(yN);

    // pD1
    #if INVERSEMAP_DEBUG_SIMPLIFY_BIDIR_MUT_DELETE_ALL
    const auto pD1 = 1_f;
    #else
    TwoTailedGeometricDist removedPathVertexNumDist(2);
    removedPathVertexNumDist.Configure(1, Math::Max(1, xN - yN), xN);
    const auto pD1 = removedPathVertexNumDist.EvaluatePDF(subspace.bidir.kd);
    #endif

    // pD2
    //#if INVERSEMAP_DEBUG_SIMPLIFY_BIDIR_MUT_DELETE_ALL
    //const auto pD2 = 1_f;
    //#else
#if 0
    const auto pD2 = 1_f / (Float)(xN - subspace.bidir.kd + 1);
#else
    Distribution1D distD2;
    distD2.Clear();
    for (int l = 0; l <= xN - subspace.bidir.kd; l++)
    {
        int m = l + subspace.bidir.kd - 1;
        if ((l > 0 && (x.vertices[l - 1].type & SurfaceInteractionType::S) > 0) || (m < xN - 1 && (x.vertices[m + 1].type & SurfaceInteractionType::S) > 0))
        {
            distD2.Add(0_f);
        }
        else
        {
            distD2.Add(1_f);
        }
    }
    if (distD2.Sum() == 0_f) { return 0_f; }
    distD2.Normalize();
    const auto pD2 = distD2.EvaluatePDF(subspace.bidir.dL);
#endif
    //#endif
    
    // pA2
    const auto pA2 = 1_f / (Float)(subspace.bidir.ka + 1);

    // Iterate through possible ranges
    Float sum = 0_f;
    #if INVERSEMAP_DEBUG_SIMPLIFY_BIDIR_MUT_PT
    for (int i = 0; i <= 0; i++)
    #else
    for (int i = 0; i <= subspace.bidir.ka; i++)
    #endif
    {
#if 0
        const auto f = InversemapUtils::ScalarContrb(y.EvaluateF(subspace.bidir.dL + i));
        if (f == 0_f)
        {
            continue;
        }
        const auto p = y.EvaluatePathPDF(scene, subspace.bidir.dL + i);
        assert(p.v > 0_f);
        const auto C = f / p.v;
        sum += 1_f / C;
#else
        const auto C = y.EvaluateUnweightContribution(scene, subspace.bidir.dL + i);
        if (C.Black()) { continue; }
        sum += 1_f / InversemapUtils::ScalarContrb(C);
#endif
    }

    return pD1 * pD2 * pA1 * pA2 * sum;
}

auto MLTMutationStrategy::Q_Lens(const Scene* scene, const Path& x, const Path& y, const Subspace& subspace) -> Float
{
    const int n = (int)(x.vertices.size());
    assert(n == (int)(y.vertices.size()));

    #if INVERSEMAP_MLTINVMAPFIXED_DEBUG_LENS_PERTURB_SUBSPACE_CONSISTENCY
    // Check if x and y is in the same subspace
    for (int i = 0; i < n; i++)
    {
        const auto& vx = x.vertices[i];
        const auto& vy = y.vertices[i];
        const auto tx = (vx.type & SurfaceInteractionType::S) > 0;
        const auto ty = (vy.type & SurfaceInteractionType::S) > 0;
        if (tx != ty)
        {
            __debugbreak();
        }
    }
    #endif

    // Find first D from E
    const int s = n - 1 - (int)std::distance(y.vertices.rbegin(), std::find_if(y.vertices.rbegin(), y.vertices.rend(), [](const SubpathSampler::PathVertex& v) -> bool
    {
        return (v.primitive->Type() & SurfaceInteractionType::E) == 0 && (v.primitive->Type() & SurfaceInteractionType::S) == 0;
    }));

    // Evaluate quantities
    // The most of terms are cancelled out so we only need to consider alpha_t * c_{s,t}
    const auto alpha = y.EvaluateAlpha(scene, n - s, TransportDirection::EL);
    assert(!alpha.Black());
    const auto cst   = y.EvaluateCst(s);
    if (cst.Black())
    {
        return 0_f;
    }

    return 1_f / InversemapUtils::ScalarContrb(alpha * cst);
}

auto MLTMutationStrategy::Q_Caustic(const Scene* scene, const Path& x, const Path& y, const Subspace& subspace) -> Float
{
    const int n = (int)(x.vertices.size());
    assert(n == (int)(y.vertices.size()));

    const auto alpha = y.EvaluateAlpha(scene, n-1, TransportDirection::LE);
    assert(!alpha.Black());
    const auto cst = y.EvaluateCst(n - 1);
    if (cst.Black())
    {
        return 0_f;
    }

    return 1_f / InversemapUtils::ScalarContrb(alpha * cst);
}

auto MLTMutationStrategy::Q_Multichain(const Scene* scene, const Path& x, const Path& y, const Subspace& subspace) -> Float
{
    const int n = (int)(x.vertices.size());
    assert(n == (int)(y.vertices.size()));

    const auto s = 1 + [&]() -> int
    {
        int iE = n - 1;
        int iL = iE - 1;
        while (iL - 1 >= 0 && (x.vertices[iL].type == SurfaceInteractionType::S || x.vertices[iL - 1].type == SurfaceInteractionType::S)) { iL--; }
        iL--;
        return iL;
    }();

    const auto alpha = y.EvaluateAlpha(scene, n - s, TransportDirection::EL);
    assert(!alpha.Black());
    const auto cst = y.EvaluateCst(s);
    if (cst.Black())
    {
        return 0_f;
    }

    return 1_f / InversemapUtils::ScalarContrb(alpha * cst);
}

auto MLTMutationStrategy::Q_ManifoldLens(const Scene* scene, const Path& x, const Path& y, const Subspace& subspace) -> Float
{
    const int n = (int)(x.vertices.size());
    assert(n == (int)(y.vertices.size()));

    // --------------------------------------------------------------------------------

    // Number of vertices in subpaths. n = s + 1 + t
    const int t = (int)std::distance(y.vertices.rbegin(), std::find_if(y.vertices.rbegin(), y.vertices.rend(), [](const SubpathSampler::PathVertex& v) -> bool { return (v.primitive->Type() & SurfaceInteractionType::E) == 0 && (v.primitive->Type() & SurfaceInteractionType::S) == 0; }));
    const int s = n - t - 1;

    // --------------------------------------------------------------------------------

    // Product of specular reflectances
    const auto prodFs_L = y.EvaluateSpecularReflectances(1, s, TransportDirection::LE);
    const auto prodFs_E = y.EvaluateSpecularReflectances(1, t, TransportDirection::EL);
    const auto prodFs   = prodFs_L * prodFs_E;
    if (prodFs.Black())
    {
        return 0_f;
    }

    // --------------------------------------------------------------------------------

    // Perturbation probability (using cancelling out)
    const auto pED = [&]() -> PDFVal
    {
        const auto& vE  = y.vertices[n - 1];
        const auto& vEn = y.vertices[n - 2];
        return vE.primitive->EvaluateDirectionPDF(vE.geom, vE.type, Vec3(), Math::Normalize(vEn.geom.p - vE.geom.p), false);
    }();
    if (pED.v == 0_f)
    {
        // Numerical error
        return 0_f;
    }

    // --------------------------------------------------------------------------------

    // Generalized geometry factor
    const auto multiG = [&]() -> Float
    {
        Subpath subpathL;
        for (int i = 0; i < s + 1; i++) { subpathL.vertices.push_back(y.vertices[i]); }
        const auto det = ManifoldUtils::ComputeConstraintJacobianDeterminant(subpathL);
        const auto G = RenderUtils::GeometryTerm(y.vertices[0].geom, y.vertices[1].geom);
        return det * G;
    }();

    // --------------------------------------------------------------------------------

    const auto C = prodFs * multiG / pED;
    return 1_f / InversemapUtils::ScalarContrb(C);
}

auto MLTMutationStrategy::Q_ManifoldCaustic(const Scene* scene, const Path& x, const Path& y, const Subspace& subspace) -> Float
{
    const int n = (int)(x.vertices.size());
    assert(n == (int)(y.vertices.size()));

    // --------------------------------------------------------------------------------

    // Number of vertices in subpaths. n = s + 1 + t
    const int s = (int)std::distance(y.vertices.begin(), std::find_if(y.vertices.begin(), y.vertices.end(), [](const SubpathSampler::PathVertex& v) -> bool { return (v.primitive->Type() & SurfaceInteractionType::L) == 0 && (v.primitive->Type() & SurfaceInteractionType::S) == 0; }));
    const int t = n - s - 1;

    // --------------------------------------------------------------------------------

    // Product of specular reflectances
    const auto prodFs_L = y.EvaluateSpecularReflectances(1, s, TransportDirection::LE);
    const auto prodFs_E = y.EvaluateSpecularReflectances(1, t, TransportDirection::EL);
    const auto prodFs = prodFs_L * prodFs_E;
    if (prodFs.Black())
    {
        return 0_f;
    }

    // --------------------------------------------------------------------------------

    // Perturbation probability (using cancelling out)
    const auto pLD = [&]() -> PDFVal
    {
        const auto& vL = y.vertices[0];
        const auto& vLn = y.vertices[1];
        return vL.primitive->EvaluateDirectionPDF(vL.geom, vL.type, Vec3(), Math::Normalize(vLn.geom.p - vL.geom.p), false);
    }();
    if (pLD.v == 0_f)
    {
        // Numerical error
        return 0_f;
    }

    // --------------------------------------------------------------------------------

    // Generalized geometry factor
    const auto multiG = [&]() -> Float
    {
        Subpath subpathE;
        for (int i = 0; i < t + 1; i++) { subpathE.vertices.push_back(y.vertices[n-1-i]); }
        const auto det = ManifoldUtils::ComputeConstraintJacobianDeterminant(subpathE);
        const auto G = RenderUtils::GeometryTerm(y.vertices[n-1].geom, y.vertices[n-2].geom);
        return det * G;
    }();

    // --------------------------------------------------------------------------------

    const auto C = prodFs * multiG / pLD;
    return 1_f / InversemapUtils::ScalarContrb(C);
}

auto MLTMutationStrategy::Q_Manifold(const Scene* scene, const Path& x, const Path& y, const Subspace& subspace) -> Float
{
    const int n = (int)(x.vertices.size());
    assert(n == (int)(y.vertices.size()));

    // --------------------------------------------------------------------------------

    // Product of specular reflectances
    const auto prodFs_L = y.EvaluateSpecularReflectances(subspace.manifold.ia+1, subspace.manifold.ib, TransportDirection::LE);
    const auto prodFs_E = y.EvaluateSpecularReflectances(n-1-(subspace.manifold.ic-1), n-1-subspace.manifold.ib, TransportDirection::EL);
    const auto prodFs = prodFs_L * prodFs_E;
    if (prodFs.Black())
    {
        return 0_f;
    }

    // --------------------------------------------------------------------------------

    // Perturbation probability (using cancelling out)
    const auto pLD = [&]() -> PDFVal
    {
        const auto& vL = y.vertices[subspace.manifold.ia];
        const auto& vLn = y.vertices[subspace.manifold.ia+1];
        return vL.primitive->EvaluateDirectionPDF(vL.geom, vL.type, Vec3(), Math::Normalize(vLn.geom.p - vL.geom.p), false);
    }();
    if (pLD.v == 0_f)
    {
        // Numerical error
        return 0_f;
    }

    // --------------------------------------------------------------------------------

    const auto fs = [&]() -> SPD
    {
        const auto* vL = &y.vertices[subspace.manifold.ib];
        const auto* vLn = &y.vertices[subspace.manifold.ib + 1];
        const auto* vLp = &y.vertices[subspace.manifold.ib - 1];
        const auto* vE = &y.vertices[subspace.manifold.ic];
        const auto* vEn = &y.vertices[subspace.manifold.ic - 1];
        const auto* vEp = subspace.manifold.ic + 1  < n ? &y.vertices[subspace.manifold.ic + 1] : nullptr;
        const auto fsL = vL->primitive->EvaluateDirection(vL->geom, vL->type, Math::Normalize(vLp->geom.p - vL->geom.p), Math::Normalize(vLn->geom.p - vL->geom.p), TransportDirection::LE, true);
        const auto fsE = vE->primitive->EvaluateDirection(vE->geom, vE->type, vEp ? Math::Normalize(vEp->geom.p - vE->geom.p) : Vec3(), Math::Normalize(vEn->geom.p - vE->geom.p), TransportDirection::EL, true);
        return fsL * fsE;
    }();
    if (fs.Black())
    {
        return 0_f;
    }

    // --------------------------------------------------------------------------------

    // Generalized geometry factor
    const auto multiG = [&]() -> Float
    {
        if (subspace.manifold.ib + 1 == subspace.manifold.ic)
        {
            return RenderUtils::GeometryTerm(y.vertices[subspace.manifold.ib].geom, y.vertices[subspace.manifold.ic].geom);
        }
        else
        {
            Subpath subpathE;
            subpathE.vertices.clear();
            for (int i = subspace.manifold.ic; i >= subspace.manifold.ib; i--) { subpathE.vertices.push_back(y.vertices[i]); }
            const auto det = ManifoldUtils::ComputeConstraintJacobianDeterminant(subpathE);
            const auto G = RenderUtils::GeometryTerm(y.vertices[subspace.manifold.ic].geom, y.vertices[subspace.manifold.ic - 1].geom);
            return det * G;
        }
        LM_UNREACHABLE();
        return 0_f;
    }();

    // --------------------------------------------------------------------------------

    const auto C = prodFs * fs * multiG / pLD;
    return 1_f / InversemapUtils::ScalarContrb(C);
}

// --------------------------------------------------------------------------------

#if INVERSEMAP_DEBUG_MLT_MANIFOLDWALK_STAT
auto MLTMutationStrategy::PrintStat() -> void
{
    if (manifoldWalkCount > 0)
    {
        const double rate = (double)manifoldWalkSuccessCount / manifoldWalkCount;
        LM_LOG_INFO(boost::str(boost::format("Manifold walk success rate: %.5f (%d / %d)") % rate % (long long)manifoldWalkSuccessCount % (long long)manifoldWalkCount));
    }
}
#endif

LM_NAMESPACE_END
