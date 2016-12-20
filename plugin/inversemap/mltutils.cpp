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

    auto PerturbRasterPos(const Path& currP, Random& rng) -> boost::optional<Vec2>
    {
        // Calculating raster position from the path have small correlated error so just iterating
        // update can change the state of the path. This affect the mixing of the chain especially when
        // the kernel size is relatively small. However for moderately bigger kernels, this effect is negilible.
        // Essentially this can happen with technique with inverse mapping, because calculating raster position
        // is a process of inverse of CDF^-1 for the directing sampling of the camera rays.
        const auto rasterPos = currP.RasterPosition();
        const auto s1 = 1_f / 256_f;
        const auto s2 = 1_f / 16_f;
        //const auto s1 = 1_f / 4096_f;
        //const auto s2 = 1_f / 256_f;
        const auto rX = Perturb(rng, rasterPos.x, s1, s2);
        const auto rY = Perturb(rng, rasterPos.y, s1, s2);
        // Immediately reject if the proposed raster position is outside of [0,1]^2
        if (rX < 0_f || rX > 1_f || rY < 0_f || rY > 1_f)
        {
            return boost::none;
        }
        return Vec2(rX, rY);
    }

    auto PerturbDSample(const Path& currP, Random& rng, int i, TransportDirection transDir) -> boost::optional<Vec2>
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
        const auto s1 = 1_f / 256_f;
        const auto s2 = 1_f / 16_f;
        const auto u1 = Perturb(rng, currU.x, s1, s2);
        const auto u2 = Perturb(rng, currU.y, s1, s2);
        return Vec2(u1, u2);
    }

    auto PerturbDirectionSample(const Path& currP, Random& rng, const Primitive* primitive, int i, TransportDirection transDir) -> boost::optional<Vec2>
    {
        assert((primitive->Type() & SurfaceInteractionType::S) == 0);
        #if INVERSEMAP_DEBUG_SIMPLIFY_INDEPENDENT
        return rng.Next2D();
        #else
        if ((primitive->Type() & SurfaceInteractionType::E) > 0)
        {
            // Perturb raster position
            const auto propRasterPos = PerturbRasterPos(currP, rng);
            if (!propRasterPos)
            {
                return boost::none;
            }
            return propRasterPos;
        }
        else
        {
            // Perturb direction sample
            const auto propDSample = PerturbDSample(currP, rng, i, transDir);
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
    std::regex reg(R"x(^LS+[DG]S*E$)x");
    std::smatch match;
    if (!std::regex_match(type, match, reg)) { return false; }
    return true;
}

auto MLTMutationStrategy::CheckMutatable_ManifoldCaustic(const Path& currP) -> bool
{
    const auto type = currP.PathType();
    std::regex reg(R"x(^LS*[DG]S+E$)x");
    std::smatch match;
    if (!std::regex_match(type, match, reg)) { return false; }
    return true;
}

auto MLTMutationStrategy::CheckMutatable_Manifold(const Path& currP) -> bool
{
    const auto type = currP.PathType();
    std::regex reg(R"x(^L[DSG]*[DG][DSG]*E$)x");
    std::smatch match;
    if (!std::regex_match(type, match, reg)) { return false; }
    return true;
}

// --------------------------------------------------------------------------------

auto MLTMutationStrategy::Mutate_Bidir(const Scene* scene, Random& rng, const Path& currP) -> boost::optional<Prop>
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
    const int dL = Math::Clamp((int)(rng.Next() * (n - kd + 1)), 0, n - kd);
    const int dM = dL + kd - 1;

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

    prop.subspace.bidir.kd = kd;
    prop.subspace.bidir.dL = dL;
    return prop;
}

auto MLTMutationStrategy::Mutate_Lens(const Scene* scene, Random& rng, const Path& currP) -> boost::optional<Prop>
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
                    const auto propU = PerturbDirectionSample(currP, rng, primitive, numVertices - 2, TransportDirection::EL);
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

auto MLTMutationStrategy::Mutate_Caustic(const Scene* scene, Random& rng, const Path& currP) -> boost::optional<Prop>
{
    const int n = (int)(currP.vertices.size());

    // Check if the strategy can mutate the current path
    // Acceptable path type: D/L S* D/G E
    const auto iL = [&]() -> boost::optional<int>
    {
        int iE = n-1;
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
        SubpathSampler::TraceSubpathFromEndpointWithSampler(scene, &subpathL.vertices[*iL], *iL > 0 ? &subpathL.vertices[*iL-1] : nullptr, *iL+1, n, TransportDirection::LE,
            [&](int numVertices, const Primitive* primitive, SubpathSampler::SampleUsage usage, int index) -> Float
            {
                if (primitive && usage == SubpathSampler::SampleUsage::Direction && (primitive->Type() & SurfaceInteractionType::S) == 0)
                {
                    assert(*iL == numVertices - 2);
                    const auto propU = PerturbDirectionSample(currP, rng, primitive, numVertices - 2, TransportDirection::LE);
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
                    const auto currVT = (currP.vertices[numVertices-1].primitive->Type() & SurfaceInteractionType::S) > 0;
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
    subpathE.vertices.push_back(currP.vertices[n-1]);

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

auto MLTMutationStrategy::Mutate_Multichain(const Scene* scene, Random& rng, const Path& currP) -> boost::optional<Prop>
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
                    const auto propU = PerturbDirectionSample(currP, rng, primitive, numVertices - 2, TransportDirection::EL);
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

auto MLTMutationStrategy::Mutate_ManifoldLens(const Scene* scene, Random& rng, const Path& currP) -> boost::optional<Prop>
{
    // L | S* | DS+E
    const int n = (int)(currP.vertices.size());

    // --------------------------------------------------------------------------------

    #pragma region Check if current path can be mutated with the strategy
    {
        const auto type = currP.PathType();
        std::regex reg(R"x(^LS+[DG]S*E$)x");
        std::smatch match;
        if (!std::regex_match(type, match, reg))
        {
            return boost::none;
        }
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
                    const auto propU = PerturbDirectionSample(currP, rng, primitive, numVertices - 2, TransportDirection::EL);
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

auto MLTMutationStrategy::Mutate_ManifoldCaustic(const Scene* scene, Random& rng, const Path& currP) -> boost::optional<Prop>
{
    // LS+D | S* | E
    const int n = (int)(currP.vertices.size());

    // --------------------------------------------------------------------------------

    #pragma region Check if current path can be mutated with the strategy
    {
        const auto type = currP.PathType();
        std::regex reg(R"x(^LS*[DG]S+E$)x");
        std::smatch match;
        if (!std::regex_match(type, match, reg))
        {
            return boost::none;
        }
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
                    const auto propU = PerturbDirectionSample(currP, rng, primitive, numVertices - 2, TransportDirection::LE);
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

auto MLTMutationStrategy::Mutate_Manifold(const Scene* scene, Random& rng, const Path& currP) -> boost::optional<Prop>
{
    const int n = (int)(currP.vertices.size());

    // --------------------------------------------------------------------------------

    #pragma region Check if current path can be mutated with the strategy
    {
        const auto type = currP.PathType();
        std::regex reg(R"x(^L[DSG]*[DG][DSG]*E$)x");
        std::smatch match;
        if (!std::regex_match(type, match, reg))
        {
            return boost::none;
        }
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
        const auto nonSIndices = [&]() -> std::vector<int>
        {
            std::vector<int> indices;
            for (size_t i = 0; i < currP.vertices.size(); i++)
            {
                const auto& v = currP.vertices[i];
                if ((v.primitive->Type() & SurfaceInteractionType::S) == 0)
                {
                    indices.push_back((int)i);
                }
            }
            return indices;
        }();
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
    const auto subpathL = [&]() -> boost::optional<Subpath>
    {
        Subpath subpathL;
        for (int i = 0; i <= subspace->ia; i++) { subpathL.vertices.push_back(currP.vertices[i]); }

        // Trace subpath
        bool failed = false;
        SubpathSampler::TraceSubpathFromEndpointWithSampler(scene, &subpathL.vertices.back(), nullptr, (int)subpathL.vertices.size(), n, TransportDirection::LE,
            [&](int numVertices, const Primitive* primitive, SubpathSampler::SampleUsage usage, int index) -> Float
            {
                if (primitive && usage == SubpathSampler::SampleUsage::Direction && (primitive->Type() & SurfaceInteractionType::S) == 0)
                {
                    const auto propU = PerturbDirectionSample(currP, rng, primitive, numVertices - 2, TransportDirection::LE);
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
            // Sampling is failed if the last vertex is S or point at infinity, or no change in number of vertices
            const auto& vL = subpathL.vertices.back();
            if (vL.geom.infinite || (vL.primitive->Type() & SurfaceInteractionType::S) > 0 || (int)subpathL.vertices.size() != subspace->ib+1)
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
    const auto subpathE = [&]() -> boost::optional<Subpath>
    {
        // Partial subpath [ib,ic]
        Subpath subpathE_Orig;
        for (int i = subspace->ic; i >= subspace->ib; i--) { subpathE_Orig.vertices.push_back(currP.vertices[i]); }
        
        // Conenct
        const auto connPath = [&]() -> boost::optional<Subpath>
        {
            if (subspace->ib + 1 == subspace->ic)
            {
                // Path connection
                const auto& vL = subpathL->vertices.back();
                const auto& vE = currP.vertices[subspace->ic];
                if (vL.geom.infinite || vE.geom.infinite)  { return boost::none; }
                if (!scene->Visible(vL.geom.p, vE.geom.p)) { return boost::none; }
                Subpath connPath;
                connPath.vertices.push_back(vE);
                connPath.vertices.push_back(vL);
                return connPath;
            }
            else
            {
                // Manifold walk
                #if INVERSEMAP_DEBUG_MLT_MANIFOLDWALK_STAT
                manifoldWalkCount++;
                #endif
                const auto connPath = ManifoldUtils::WalkManifold(scene, subpathE_Orig, subpathL->vertices.back().geom.p);
                if (!connPath) { return boost::none; }
                const auto connPathInv = ManifoldUtils::WalkManifold(scene, *connPath, subpathE_Orig.vertices.back().geom.p);
                if (!connPathInv) { return boost::none; }
                #if INVERSEMAP_DEBUG_MLT_MANIFOLDWALK_STAT
                manifoldWalkSuccessCount++;
                #endif
                return connPath;
            }
            LM_UNREACHABLE();
            return boost::none;
        }();
        if (!connPath)
        {
            return boost::none;
        }

        // Connected eye subpath
        Subpath subpathE;
        for (int i = n - 1; i > subspace->ic; i--) { subpathE.vertices.push_back(currP.vertices[i]); }
        for (const auto& v : connPath->vertices) { subpathE.vertices.push_back(v); }

        return subpathE;
    }();
    if (!subpathE)
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

auto MLTMutationStrategy::Q_Bidir(const Scene* scene, const Path& x, const Path& y, const Subspace& subspace) -> Float
{
    Float sum = 0_f;
    #if INVERSEMAP_DEBUG_SIMPLIFY_BIDIR_MUT_PT
    for (int i = 0; i <= 0; i++)
    #else
    for (int i = 0; i <= subspace.bidir.kd; i++)
    #endif
    {
        const auto f = InversemapUtils::ScalarContrb(y.EvaluateF(subspace.bidir.dL + i));
        if (f == 0_f)
        {
            continue;
        }
        const auto p = y.EvaluatePathPDF(scene, subspace.bidir.dL + i);
        assert(p.v > 0_f);
        const auto C = f / p.v;
        sum += 1_f / C;
    }
    return sum;
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
            for (int i = subspace.manifold.ic; i >= subspace.manifold.ib; i--) { subpathE.vertices.push_back(y.vertices[i]); }
            const auto det = ManifoldUtils::ComputeConstraintJacobianDeterminant(subpathE);
            const auto G = RenderUtils::GeometryTerm(y.vertices[subspace.manifold.ic].geom, y.vertices[subspace.manifold.ic - 1].geom);
            return det * G;
        }
        LM_UNREACHABLE();
        return 0_f;
    }();

    // --------------------------------------------------------------------------------

    const auto C = prodFs * multiG / pLD;
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
