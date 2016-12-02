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
        }
        else
        {
            r = (r - 0.5_f) * 2_f;
            result = u - s2 * std::exp(-std::log(s2 / s1) * r);
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

    prop.kd = kd;
    prop.dL = dL;
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
    // L | S+ | DS+E
    const int n = (int)(currP.vertices.size());

    // --------------------------------------------------------------------------------

    #pragma region Check if current path can be mutated with the strategy
    {
        const auto type = currP.PathType();
        std::regex reg(R"x(^LS+DS*E$)x");
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
            }
        );
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
    {
        LM_LOG_DEBUG("manifoldlens_perturbed_eye_subpath");
        DebugIO::Wait();
        std::vector<double> vs;
        for (const auto& v : subpathE->vertices) for (int i = 0; i < 3; i++) vs.push_back(v.geom.p[i]);
        std::stringstream ss;
        {
            cereal::JSONOutputArchive oa(ss);
            oa(vs);
        }
        DebugIO::Output("manifoldlens_perturbed_eye_subpath", ss.str());
    }
    #endif

    // --------------------------------------------------------------------------------

    #pragma region Connect light subapth
    const auto subpathL = [&]() -> boost::optional<Subpath>
    {
        // Original light subpath (LS+D)
        Subpath subpathL_Orig;
        const int nE = (int)(subpathE->vertices.size());
        const int nL = n - nE;
        for (int s = 0; s < nL + 1; s++)
        {
            subpathL_Orig.vertices.push_back(currP.vertices[s]);
        }

        // Manifold walk
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
    return prop;
}

// --------------------------------------------------------------------------------

auto MLTMutationStrategy::Q_Bidir(const Scene* scene, const Path& x, const Path& y, int kd, int dL) -> Float
{
    Float sum = 0_f;
    #if INVERSEMAP_DEBUG_SIMPLIFY_BIDIR_MUT_PT
    for (int i = 0; i <= 0; i++)
    #else
    for (int i = 0; i <= kd; i++)
    #endif
    {
        const auto f = InversemapUtils::ScalarContrb(y.EvaluateF(dL + i));
        if (f == 0_f)
        {
            continue;
        }
        const auto p = y.EvaluatePathPDF(scene, dL + i);
        assert(p.v > 0_f);
        const auto C = f / p.v;
        sum += 1_f / C;
    }
    return sum;
}

auto MLTMutationStrategy::Q_Lens(const Scene* scene, const Path& x, const Path& y, int kd, int dL) -> Float
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

auto MLTMutationStrategy::Q_Caustic(const Scene* scene, const Path& x, const Path& y, int kd, int dL) -> Float
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

auto MLTMutationStrategy::Q_Multichain(const Scene* scene, const Path& x, const Path& y, int kd, int dL) -> Float
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

auto MLTMutationStrategy::Q_ManifoldLens(const Scene* scene, const Path& x, const Path& y, int kd, int dL) -> Float
{
    const int n = (int)(x.vertices.size());
    assert(n == (int)(y.vertices.size()));

    // --------------------------------------------------------------------------------

    // Number of vertices in subpaths. n = s + 1 + t
    const int t = (int)std::distance(y.vertices.rbegin(), std::find_if(y.vertices.rbegin(), y.vertices.rend(), [](const SubpathSampler::PathVertex& v) -> bool { return (v.primitive->Type() & SurfaceInteractionType::E) == 0 && (v.primitive->Type() & SurfaceInteractionType::S) == 0; }));
    const int s = n - t - 1;

    // --------------------------------------------------------------------------------

    // Product of specular reflectances
    const auto EvaluateSpecularReflectances = [&](int l, TransportDirection transDir) -> SPD
    {
        SPD prodFs(1_f);
        const auto index = [&](int i) { return transDir == TransportDirection::LE ? i : n - 1 - i; };
        for (int i = 1; i < l; i++)
        {
            const auto& vi  = y.vertices[index(i)];
            const auto& vip = y.vertices[index(i - 1)];
            const auto& vin = y.vertices[index(i + 1)];
            assert(vi.type == SurfaceInteractionType::S);
            const auto wi = Math::Normalize(vip.geom.p - vi.geom.p);
            const auto wo = Math::Normalize(vin.geom.p - vi.geom.p);
            const auto fs = vi.primitive->EvaluateDirection(vi.geom, vi.type, wi, wo, transDir, false);
            const auto fsInv = vi.primitive->EvaluateDirection(vi.geom, vi.type, wo, wi, (TransportDirection)(1 - (int)transDir), false);
            // Sometimes evaluation in the swapped directions wrongly evaluates as total internal reflection.
            // Reject such a case here.
            if (fsInv.Black()) { return SPD(); }
            prodFs *= fs;
        }
        return prodFs;
    };
    const auto prodFs_L = EvaluateSpecularReflectances(s, TransportDirection::LE);
    const auto prodFs_E = EvaluateSpecularReflectances(t, TransportDirection::EL);
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

LM_NAMESPACE_END
