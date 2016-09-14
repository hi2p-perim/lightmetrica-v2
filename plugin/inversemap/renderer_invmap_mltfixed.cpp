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

#include "inversemaputils.h"
#include <mutex>

#define INVERSEMAP_MLTFIXED_DEBUG_OUTPUT_TRIANGLES 1
#define INVERSEMAP_MLTFIXED_DEBUG_OUTPUT_SAMPLED_PATHS 1
#define INVERSEMAP_MLTFIXED_DEBUG_LONGEST_REJECTION 1
#define INVERSEMAP_MLTFIXED_DEBUG_SIMPLIFY_BIDIR_MUT_DELETE_ALL 0
#define INVERSEMAP_MLTFIXED_DEBUG_SIMPLIFY_BIDIR_MUT_PT 0
#define INVERSEMAP_MLTFIXED_DEBUG_SIMPLIFY_LENS_PERTURB_INDEPENDENT 0
#define INVERSEMAP_MLTINVMAPFIXED_DEBUG_LENS_PERTURB_SUBSPACE_CONSISTENCY 0

LM_NAMESPACE_BEGIN

namespace
{
    enum class Strategy : int
    {
        Bidir,
        Lens,
        Caustic,
        Multichain,
    };
}

// Bidirectional mutation first narrows the mutation space by limiting the deleted range
// in the current path, so it requires some additional information other than proposed path itself.
struct Prop
{
    Path p;
    int kd;
    int dL;
};

class MutationStrategy
{
public:

    static auto Mutate(Strategy strategy, const Scene* scene, Random& rng, const Path& currP) -> boost::optional<Prop>
    {
        if (strategy == Strategy::Bidir)           { return Mutate_Bidir(scene, rng, currP); }
        else if (strategy == Strategy::Lens)       { return Mutate_Lens(scene, rng, currP); }
        else if (strategy == Strategy::Caustic)    { return Mutate_Caustic(scene, rng, currP); }
        else if (strategy == Strategy::Multichain) { return Mutate_Multichain(scene, rng, currP); }
        LM_UNREACHABLE();
        return Prop();
    }

    static auto Q(Strategy strategy, const Scene* scene, const Path& x, const Path& y, int kd, int dL) -> Float
    {
        if (strategy == Strategy::Bidir)           { return Q_Bidir(scene, x, y, kd, dL); }
        else if (strategy == Strategy::Lens)       { return Q_Lens(scene, x, y, kd, dL); }
        else if (strategy == Strategy::Caustic)    { return Q_Caustic(scene, x, y, kd, dL); }
        else if (strategy == Strategy::Multichain) { return Q_Multichain(scene, x, y, kd, dL); }
        LM_UNREACHABLE();
        return 0_f;
    }

private:

    static auto Mutate_Bidir(const Scene* scene, Random& rng, const Path& currP) -> boost::optional<Prop>
    {
        const int n = (int)(currP.vertices.size());

        // Implements bidirectional mutation within same path length
        // Some simplification
        //   - Mutation within the same path length

        // Choose # of path vertices to be deleted
        #if INVERSEMAP_MLTFIXED_DEBUG_SIMPLIFY_BIDIR_MUT_DELETE_ALL
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
        #if INVERSEMAP_MLTFIXED_DEBUG_SIMPLIFY_BIDIR_MUT_PT
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

    static auto Mutate_Lens(const Scene* scene, Random& rng, const Path& currP) -> boost::optional<Prop>
    {
        const int n = (int)(currP.vertices.size());

        // Check if the strategy can mutate the current path
        // Acceptable path type: D/L/empty D/L S* E
        {
            int iE = n - 1;
            int iL = iE - 1;
            iL--;
            while (iL >= 0 && currP.vertices[iL].type == SurfaceInteractionType::S) { iL--; }
            if (iL > 0 && currP.vertices[iL - 1].type == SurfaceInteractionType::S) { return boost::none;  }
        }

        // Eye subpath
        const auto subpathE = [&]() -> boost::optional<Subpath>
        {
            Subpath subpathE;
            subpathE.vertices.push_back(currP.vertices[n - 1]);
            bool failed = false;

            #if !INVERSEMAP_MLTFIXED_DEBUG_SIMPLIFY_LENS_PERTURB_INDEPENDENT
            // Perturb raster position
            const auto propRasterPos = PerturbRasterPos(currP, rng);
            if (!propRasterPos)
            {
                return boost::none;
            }
            #endif

            // Trace subpath
            SubpathSampler::TraceSubpathFromEndpointWithSampler(scene, &subpathE.vertices[0], nullptr, 1, n, TransportDirection::EL,
                [&](int numVertices, const Primitive* primitive, SubpathSampler::SampleUsage usage, int index) -> Float
                {
                    // Perturb sample used for sampling direction
                    if (primitive && (primitive->Type() & SurfaceInteractionType::E) > 0 && usage == SubpathSampler::SampleUsage::Direction)
                    {
                        #if INVERSEMAP_MLTFIXED_DEBUG_SIMPLIFY_LENS_PERTURB_INDEPENDENT
                        return rng.Next();
                        #else
                        return (*propRasterPos)[index];
                        #endif
                    }
                    // Other samples are not used.
                    // TODO: This will not work for multi-component materials.
                    // Introduce a feature to fix the component type in evaluation.
                    return rng.Next();
                },
                [&](int numVertices, const Vec2& /*rasterPos*/, const SubpathSampler::SubpathSampler::PathVertex& pv, const SubpathSampler::SubpathSampler::PathVertex& v, SPD& throughput) -> bool
                {
                    if (numVertices == 1)
                    {
                        return true;
                    }
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

    static auto Mutate_Caustic(const Scene* scene, Random& rng, const Path& currP) -> boost::optional<Prop>
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

    static auto Mutate_Multichain(const Scene* scene, Random& rng, const Path& currP) -> boost::optional<Prop>
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

private:

    static auto Q_Bidir(const Scene* scene, const Path& x, const Path& y, int kd, int dL) -> Float
    {
        Float sum = 0_f;
        #if INVERSEMAP_MLTFIXED_DEBUG_SIMPLIFY_BIDIR_MUT_PT
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

    static auto Q_Lens(const Scene* scene, const Path& x, const Path& y, int kd, int dL) -> Float
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

        // Find first S from E
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

    static auto Q_Caustic(const Scene* scene, const Path& x, const Path& y, int kd, int dL) -> Float
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

    static auto Q_Multichain(const Scene* scene, const Path& x, const Path& y, int kd, int dL) -> Float
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

private:

    static auto Perturb(Random& rng, const Float u, const Float s1, const Float s2)
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

    static auto PerturbRasterPos(const Path& currP, Random& rng) -> boost::optional<Vec2>
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

    static auto PerturbDSample(const Path& currP, Random& rng, int i, TransportDirection transDir) -> boost::optional<Vec2>
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

    static auto PerturbDirectionSample(const Path& currP, Random& rng, const Primitive* primitive, int i, TransportDirection transDir) -> boost::optional<Vec2>
    {
        assert((primitive->Type() & SurfaceInteractionType::S) == 0);
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
    }

};

///! Metropolis light transport (fixed path length)
class Renderer_Invmap_MLTFixed final : public Renderer
{
public:

    LM_IMPL_CLASS(Renderer_Invmap_MLTFixed, Renderer);

public:

    int numVertices_;
    long long numMutations_;
    long long numSeedSamples_;
    MutationStrategy mut_;
    std::vector<Float> strategyWeights_{ 1_f, 1_f, 1_f, 1_f };

public:

    LM_IMPL_F(Initialize) = [this](const PropertyNode* prop) -> bool
    {
        if (!prop->ChildAs<int>("num_vertices", numVertices_)) return false;
        if (!prop->ChildAs<long long>("num_mutations", numMutations_)) return false;
        if (!prop->ChildAs<long long>("num_seed_samples", numSeedSamples_)) return false;
        {
            LM_LOG_INFO("Loading mutation strategy weights");
            LM_LOG_INDENTER();
            const auto* child = prop->Child("mutation_strategy_weights");
            if (!child)
            {
                LM_LOG_ERROR("Missing 'mutation_strategy_weights'");
                return false;
            }
            strategyWeights_[(int)(Strategy::Bidir)]      = child->ChildAs<Float>("bidir", 1_f);
            strategyWeights_[(int)(Strategy::Lens)]       = child->ChildAs<Float>("lens", 1_f);
            strategyWeights_[(int)(Strategy::Caustic)]    = child->ChildAs<Float>("caustic", 1_f);
            strategyWeights_[(int)(Strategy::Multichain)] = child->ChildAs<Float>("multichain", 1_f);
        }
        return true;
    };

    LM_IMPL_F(Render) = [this](const Scene* scene, Random* initRng, Film* film) -> void
    {
        #if INVERSEMAP_MLTFIXED_DEBUG_OUTPUT_TRIANGLES
        // Output triangles
        {
            std::ofstream out("tris.out", std::ios::out | std::ios::trunc);
            for (int i = 0; i < scene->NumPrimitives(); i++)
            {
                const auto* primitive = scene->PrimitiveAt(i);
                const auto* mesh = primitive->mesh;
                if (!mesh) { continue; }
                const auto* ps = mesh->Positions();
                const auto* faces = mesh->Faces();
                for (int fi = 0; fi < primitive->mesh->NumFaces(); fi++)
                {
                    unsigned int vi1 = faces[3 * fi];
                    unsigned int vi2 = faces[3 * fi + 1];
                    unsigned int vi3 = faces[3 * fi + 2];
                    Vec3 p1(primitive->transform * Vec4(ps[3 * vi1], ps[3 * vi1 + 1], ps[3 * vi1 + 2], 1_f));
                    Vec3 p2(primitive->transform * Vec4(ps[3 * vi2], ps[3 * vi2 + 1], ps[3 * vi2 + 2], 1_f));
                    Vec3 p3(primitive->transform * Vec4(ps[3 * vi3], ps[3 * vi3 + 1], ps[3 * vi3 + 2], 1_f));
                    out << p1.x << " " << p1.y << " " << p1.z << " "
                        << p2.x << " " << p2.y << " " << p2.z << " "
                        << p3.x << " " << p3.y << " " << p3.z << " " 
                        << p1.x << " " << p1.y << " " << p1.z << std::endl;
                }
            }
        }
        #endif

        // --------------------------------------------------------------------------------

        #pragma region Compute normalization factor
        #if INVERSEMAP_OMIT_NORMALIZATION
        const auto b = 1_f;
        #else
        const auto b = [&]() -> Float
        {
            LM_LOG_INFO("Computing normalization factor");
            LM_LOG_INDENTER();
            
            struct Context
            {
                Random rng;
                Float b = 0_f;
            };
            std::vector<Context> contexts(Parallel::GetNumThreads());
            for (auto& ctx : contexts) { ctx.rng.SetSeed(initRng->NextUInt()); }
            
            Parallel::For(numSeedSamples_, [&](long long index, int threadid, bool init)
            {
                auto& ctx = contexts[threadid];
                
                // Generate primary sample
                std::vector<Float> ps;
                for (int i = 0; i < InversemapUtils::NumSamples(numVertices_); i++)
                {
                    ps.push_back(ctx.rng.Next());
                }

                // Map to path
                const auto p = InversemapUtils::MapPS2Path(scene, ps);
                if (!p || (int)p->vertices.size() != numVertices_)
                {
                    return;
                }

                // Accumulate contribution
                ctx.b += InversemapUtils::ScalarContrb(p->EvaluateF(0) / p->EvaluatePathPDF(scene, 0));
            });

            Float b = 0_f;
            for (auto& ctx : contexts) { b += ctx.b; }
            b /= numSeedSamples_;

            LM_LOG_INFO(boost::str(boost::format("Normalization factor: %.10f") % b));
            return b;
        }();
        #endif
        #pragma endregion

        // --------------------------------------------------------------------------------

        #pragma region Rendering
        {
            LM_LOG_INFO("Rendering");
            LM_LOG_INDENTER();

            // --------------------------------------------------------------------------------

            // Thread-specific context
            struct Context
            {
                Random rng;
                Film::UniquePtr film{ nullptr, nullptr };
                Path currP;
            };
            std::vector<Context> contexts(Parallel::GetNumThreads());
            for (auto& ctx : contexts)
            {
                ctx.rng.SetSeed(initRng->NextUInt());
                ctx.film = ComponentFactory::Clone<Film>(film);

                // Initial state
                while (true)
                {
                    // Generate initial sample with positive contribution with path tracing
                    // Ignore start-up bias here
                    std::vector<Float> ps;
                    for (int i = 0; i < InversemapUtils::NumSamples(numVertices_); i++)
                    {
                        ps.push_back(initRng->Next());
                    }

                    const auto path = InversemapUtils::MapPS2Path(scene, ps);
                    if (!path || (int)path->vertices.size() != numVertices_ || path->EvaluateF(0).Black())
                    {
                        continue;
                    }

                    ctx.currP = *path;
                    break;
                }
            }

            // --------------------------------------------------------------------------------

            #if INVERSEMAP_MLTFIXED_DEBUG_LONGEST_REJECTION
            static long long maxReject = 0;
            #endif
            Parallel::For(numMutations_, [&](long long index, int threadid, bool init) -> void
            {
                auto& ctx = contexts[threadid];

                // --------------------------------------------------------------------------------

                const auto accept = [&]() -> bool
                {
                    #pragma region Select mutation strategy

                    const auto strategy = [&]() -> Strategy
                    {
                        static thread_local const auto StrategyDist = [&]() -> Distribution1D
                        {
                            Distribution1D dist;
                            for (Float w : strategyWeights_) dist.Add(w);
                            dist.Normalize();
                            return dist;
                        }();
                        return (Strategy)(StrategyDist.Sample(ctx.rng.Next()));
                    }();

                    #pragma endregion

                    // --------------------------------------------------------------------------------

                    #pragma region Mutate the current path

                    const auto prop = MutationStrategy::Mutate(strategy, scene, ctx.rng, ctx.currP);
                    if (!prop)
                    {
                        return false;
                    }

                    #pragma endregion

                    // --------------------------------------------------------------------------------

                    #pragma region MH update
                    {
                        const auto Qxy = MutationStrategy::Q(strategy, scene, ctx.currP, prop->p, prop->kd, prop->dL);
                        const auto Qyx = MutationStrategy::Q(strategy, scene, prop->p, ctx.currP, prop->kd, prop->dL);
                        Float A = 0_f;
                        if (Qxy <= 0_f || Qyx <= 0_f || std::isnan(Qxy) || std::isnan(Qyx))
                        {
                            A = 0_f;
                        }
                        else
                        {
                            A = Math::Min(1_f, Qyx / Qxy);
                        }
                        if (ctx.rng.Next() < A)
                        {
                            ctx.currP = prop->p;
                        }
                        else
                        {
                            return false;
                        }
                    }
                    #pragma endregion

                    // --------------------------------------------------------------------------------

                    return true;
                }();

                // --------------------------------------------------------------------------------

                #if INVERSEMAP_MLTFIXED_DEBUG_LONGEST_REJECTION
                if (threadid == 0)
                {
                    static bool prevIsReject = false;
                    static long long sequencialtReject = 0;
                    if (accept)
                    {
                        prevIsReject = false;
                    }
                    else
                    {
                        if (prevIsReject)
                        {
                            sequencialtReject++;
                        }
                        else
                        {
                            sequencialtReject = 1;
                        }
                        prevIsReject = true;
                        if (sequencialtReject > maxReject)
                        {
                            maxReject = sequencialtReject;
                        }
                    }
                }
                #else
                LM_UNUSED(accept);
                #endif

                // --------------------------------------------------------------------------------

                #pragma region Accumulate contribution
                {
                    const auto currF = ctx.currP.EvaluateF(0);
                    if (!currF.Black())
                    {
                        ctx.film->Splat(ctx.currP.RasterPosition(), currF * (b / InversemapUtils::ScalarContrb(currF)));
                        //ctx.film->Splat(ctx.currP.RasterPosition(), SPD(b));
                    }
                }
                #pragma endregion

                // --------------------------------------------------------------------------------

                #if INVERSEMAP_MLTFIXED_DEBUG_OUTPUT_SAMPLED_PATHS
                if (threadid == 0)
                {
                    // Output sampled path
                    static long long count = 0;
                    if (count == 0)
                    {
                        boost::filesystem::remove("dirs.out");
                    }
                    if (count < 100 && accept)
                    {
                        count++;
                        std::ofstream out("dirs.out", std::ios::out | std::ios::app);
                        for (const auto& v : ctx.currP.vertices)
                        {
                            out << boost::str(boost::format("%.10f %.10f %.10f ") % v.geom.p.x % v.geom.p.y % v.geom.p.z);
                        }
                        out << std::endl;
                    }
                }
                #endif
            });

            
            // --------------------------------------------------------------------------------

            #if INVERSEMAP_MLTFIXED_DEBUG_LONGEST_REJECTION
            {
                LM_LOG_INFO("Maximum # of rejection: " + std::to_string(maxReject));
            }
            #endif

            // --------------------------------------------------------------------------------

            #pragma region Gather & Rescale
            film->Clear();
            for (auto& ctx : contexts)
            {
                film->Accumulate(ctx.film.get());
            }
            film->Rescale((Float)(film->Width() * film->Height()) / numMutations_);
            #pragma endregion
        }
        #pragma endregion

    };

};

LM_COMPONENT_REGISTER_IMPL(Renderer_Invmap_MLTFixed, "renderer::invmap_mltfixed");

LM_NAMESPACE_END
