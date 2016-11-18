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

LM_NAMESPACE_BEGIN

namespace
{
    struct VertexConstraintJacobian
    {
        Mat2 A;
        Mat2 B;
        Mat2 C;
    };

    using ConstraintJacobian = std::vector<VertexConstraintJacobian>;

	auto ComputeConstraintJacobian(const Path& path, ConstraintJacobian& nablaC) -> void
	{
		const int n = (int)(path.vertices.size());
		for (int i = 1; i < n - 1; i++)
		{
			#pragma region Some precomputation

			const auto& x  = path.vertices[i].geom;
			const auto& xp = path.vertices[i-1].geom;
			const auto& xn = path.vertices[i+1].geom;
			
			const auto wi = Math::Normalize(xp.p - x.p);
			const auto wo = Math::Normalize(xn.p - x.p);
			const auto H  = Math::Normalize(wi + wo);

			const auto inv_wiL = 1_f / Math::Length(xp.p - x.p);
			const auto inv_woL = 1_f / Math::Length(xn.p - x.p);
			const auto inv_HL  = 1_f / Math::Length(wi + wo);
			
			const auto dot_H_n    = Math::Dot(x.sn, H);
			const auto dot_H_dndu = Math::Dot(x.dndu, H);
			const auto dot_H_dndv = Math::Dot(x.dndv, H);
			const auto dot_u_n    = Math::Dot(x.dpdu, x.sn);
			const auto dot_v_n    = Math::Dot(x.dpdv, x.sn);

			const auto s = x.dpdu - dot_u_n * x.sn;
			const auto t = x.dpdv - dot_v_n * x.sn;

			const auto div_inv_wiL_HL = inv_wiL * inv_HL;
			const auto div_inv_woL_HL = inv_woL * inv_HL;

			#pragma endregion

			// --------------------------------------------------------------------------------

			#pragma region Compute $A_i$ (derivative w.r.t. $x_{i-1}$)
			
			{
				const auto tu = (xp.dpdu - wi * Math::Dot(wi, xp.dpdu)) * div_inv_wiL_HL;
				const auto tv = (xp.dpdv - wi * Math::Dot(wi, xp.dpdv)) * div_inv_wiL_HL;
				const auto dHdu = tu - H * Math::Dot(tu, H);
				const auto dHdv = tv - H * Math::Dot(tv, H);
				nablaC[i-1].A = Mat2(
					Math::Dot(dHdu, s), Math::Dot(dHdu, t),
					Math::Dot(dHdv, s), Math::Dot(dHdv, t));
			}

			#pragma endregion
			
			// --------------------------------------------------------------------------------

			#pragma region Compute $B_i$ (derivative w.r.t. $x_i$)

			{
				const auto tu = -x.dpdu * (div_inv_wiL_HL + div_inv_woL_HL) + wi * (Math::Dot(wi, x.dpdu) * div_inv_wiL_HL) + wo * (Math::Dot(wo, x.dpdu) * div_inv_woL_HL);
				const auto tv = -x.dpdv * (div_inv_wiL_HL + div_inv_woL_HL) + wi * (Math::Dot(wi, x.dpdv) * div_inv_wiL_HL) + wo * (Math::Dot(wo, x.dpdv) * div_inv_woL_HL);
				const auto dHdu = tu - H * Math::Dot(tu, H);
				const auto dHdv = tv - H * Math::Dot(tv, H);
				nablaC[i-1].B = Mat2(
					Math::Dot(dHdu, s) - Math::Dot(x.dpdu, x.dndu) * dot_H_n - dot_u_n * dot_H_dndu,
					Math::Dot(dHdu, t) - Math::Dot(x.dpdv, x.dndu) * dot_H_n - dot_v_n * dot_H_dndu,
					Math::Dot(dHdv, s) - Math::Dot(x.dpdu, x.dndv) * dot_H_n - dot_u_n * dot_H_dndv,
					Math::Dot(dHdv, t) - Math::Dot(x.dpdv, x.dndv) * dot_H_n - dot_v_n * dot_H_dndv);
			}

			#pragma endregion
			
			// --------------------------------------------------------------------------------

			#pragma region Compute $C_i$ (derivative w.r.t. $x_{i+1}$)

			{
				const auto tu = (xn.dpdu - wo * Math::Dot(wo, xn.dpdu)) * div_inv_woL_HL;
				const auto tv = (xn.dpdv - wo * Math::Dot(wo, xn.dpdv)) * div_inv_woL_HL;
				const auto dHdu = tu - H * Math::Dot(tu, H);
				const auto dHdv = tv - H * Math::Dot(tv, H);
				nablaC[i - 1].C = Mat2(
					Math::Dot(dHdu, s), Math::Dot(dHdu, t),
					Math::Dot(dHdv, s), Math::Dot(dHdv, t));
			}

			#pragma endregion
		}
	}

    auto SolveBlockLinearEq(const ConstraintJacobian& nablaC, const std::vector<Vec2>& V, std::vector<Vec2>& W) -> void
	{
		const int n = (int)(nablaC.size());
		assert(V.size() == nablaC.size());
		
		// --------------------------------------------------------------------------------

		#pragma region LU decomposition

		// A'_{0,n-1} = B_{0,n-1}
		// B'_{0,n-2} = C_{0,n-2}
		// C'_{0,n-2} = A_{1,n-1}

		std::vector<Mat2> L(n);
		std::vector<Mat2> U(n);
		{
			// $U_1 = A'_1$
			U[0] = nablaC[0].B;
			for (int i = 1; i < n; i++)
			{
				L[i] = nablaC[i].A * Math::Inverse(U[i-1]);		// $L_i = C'_i U_{i-1}^-1$
				U[i] = nablaC[i].B - L[i] * nablaC[i-1].C;		// $U_i = A'_i - L_i * B'_{i-1}$
			}
		}

		#pragma endregion

		// --------------------------------------------------------------------------------

		#pragma region Forward substitution
 
		// Solve $L V' = V$
		std::vector<Vec2> Vp(n);
		Vp[0] = V[0];
		for (int i = 1; i < n; i++)
		{
			// V'_i = V_i - L_i V'_{i-1}
			Vp[i] = V[i] - L[i] * Vp[i - 1];
		}

		#pragma endregion

		// --------------------------------------------------------------------------------

		#pragma region Backward substitution

		W.assign(n, Vec2());

		// Solve $U_n W_n = V'_n$
		W[n - 1] = Math::Inverse(U[n - 1]) * Vp[n - 1];

		for (int i = n - 2; i >= 0; i--)
		{
			// Solve $U_i W_i = V'_i - V_i W_{i+1}$
			W[i] = Math::Inverse(U[i]) * (Vp[i] - V[i] * W[i + 1]);
		}

		#pragma endregion
	}

	auto WalkManifold(const Scene& scene, const Path& seedPath, const Vec3& target, Path& outPath) -> bool
	{
		#pragma region Preprocess

		// Number of path vertices
		const int n = (int)(seedPath.vertices.size());

		// Initial path
		Path currPath = seedPath;

		// Compute $\nabla C$
		ConstraintJacobian nablaC(n - 2);
		ComputeConstraintJacobian(currPath, nablaC);

		// Compute $L$
		Float L = 0;
		for (const auto& x : currPath.vertices)
		{
			L = Math::Max(L, Math::Length(x.geom.p));
		}

		#pragma endregion

		// --------------------------------------------------------------------------------

		#pragma region Optimization loop

		int iter = 0;
		const Float MaxBeta = 100.0;
		Float beta = MaxBeta;
		const Float Eps = 10e-5;
		const int MaxIter = 30;
		bool converged = false;

		while (true)
		{
			#pragma region Stop condition

			if (iter++ >= MaxIter)
			{
				break;
			}

			if (Math::Length(currPath.vertices[n - 1].geom.p - target) < Eps * L)
			{
				converged = true;
				break;
			}

			#pragma endregion

			// --------------------------------------------------------------------------------

			#pragma region Compute movement in tangement plane

			// New position of initial specular vertex
			Vec3 p;
			{
				// $x_n$, $x'_n$
				const auto& xn = currPath.vertices[n - 1].geom.p;
				const auto& xnp = target;

				// $T(x_n)^T$
				const Math::dmat3x2 TxnT = Math::transpose(Math::dmat2x3(currPath.vertices[n - 1].geom.dpdu, currPath.vertices[n - 1].geom.dpdv));

				// $V \equiv B_n T(x_n)^T (x'_n - x)$
				const auto Bn_n2p = nablaC[n - 3].C;
				const auto V_n2p = Bn_n2p * TxnT * (xnp - xn);

				// Solve $AW = V$
				std::vector<Vec2> V(n - 2);
				std::vector<Vec2> W(n - 2);
				for (int i = 0; i < n - 2; i++) { V[i] = i == n - 3 ? V_n2p : Vec2(); }
				SolveBlockLinearEq(nablaC, V, W);

				// $x_2$, $T(x_2)$
				const auto& x2 = currPath.vertices[1].geom.p;
				const Math::dmat2x3 Tx2(currPath.vertices[1].geom.dpdu, currPath.vertices[1].geom.dpdv);

				// $W_{n-2} = P_2 W$
				const auto Wn2p = W[n - 3];

				// $p = x_2 - \beta T(x_2) P_2 W_{n-2}$
				p = x2 - beta * Tx2 * Wn2p;
			}

			#pragma endregion

			// --------------------------------------------------------------------------------

			#pragma region Propagate light path to $p - x1$

			bool fail = false;

			Path nextPath;

			// Initial vertex
			nextPath.vertices.push_back(currPath.vertices[0]);

			for (int i = 0; i < n - 1; i++)
			{
				// Current vertex & previous vertex
				const auto* v  = &nextPath.vertices[i];
				const auto* vp = i > 0 ? &nextPath.vertices[i-1] : nullptr;

				// Next ray direction
				Vec3 wo;
				if (i == 0)
				{
					wo = Math::normalize(p - currPath.vertices[0].geom.p);
				}
				else
				{
					v->primitive->SampleDirection(Vec2(), 0, v->type, v->geom, Math::normalize(vp->geom.p - v->geom.p), wo);
				}

				// Intersection query
				Ray ray = { v->geom.p, wo };
				Intersection isect;
				if (!scene.Intersect(ray, isect))
				{
					fail = true;
					break;
				}

				// Fails if not intersected with specular vertex
				if (i < n - 2 && (isect.Prim->Type & PrimitiveType::S) == 0)
				{
					fail = true;
					break;
				}

				// Create a new vertex
				PathVertex vn;
				vn.geom = isect.geom;
				vn.type = isect.Prim->Type;
				vn.primitive = isect.Prim;
				nextPath.vertices.push_back(vn);
			}
			
			if (!fail)
			{
				if (nextPath.vertices.size() != currPath.vertices.size())
				{
					// # of vertices is different
					fail = true;
				}
				else if ((nextPath.vertices.back().type & PrimitiveType::D) == 0)
				{
					// Last vertex type is not D
					fail = true;
				}
				else
				{
					// Larger difference
					const auto d  = Math::Length2(currPath.vertices.back().geom.p - target);
					const auto dn = Math::Length2(nextPath.vertices.back().geom.p - target);
					if (dn >= d)
					{
						fail = true;
					}
				}
			}

			#pragma endregion

			// --------------------------------------------------------------------------------

			#pragma region Update beta

			if (fail)
			{
				beta *= 0.5;
			}
			else
			{
				beta = Math::min(MaxBeta, beta * 1.7);
				//beta = Math::min(MaxBeta, beta * 2.0);
				currPath = nextPath;
			}

			#pragma endregion
		}

		#pragma endregion

		// --------------------------------------------------------------------------------

		outPath = currPath;
		assert(seedPath.vertices.size() == outPath.vertices.size());

		return converged;
	}
}

class Renderer_Debug_ManifoldWalk final : public Renderer
{
public:

    LM_IMPL_CLASS(Renderer_Debug_ManifoldWalk, Renderer);

public:

    LM_IMPL_F(Initialize) = [this](const PropertyNode* prop) -> bool
    {
        return true;
    };

    LM_IMPL_F(Render) = [this](const Scene* scene, Random* initRng, const std::string& outputPath) -> void
    {
        // Sample a light subpath 
        Subpath subpathL;
        {
            // 1
            {
                SubpathSampler::PathVertex v;
                v.type = SurfaceInteractionType::L;
                v.primitive = scene->SampleEmitter(v.type, 0_f);

                Vec3 _;
                v.primitive->SamplePositionAndDirection(Vec2(), Vec2(), v.geom, _);
                v.geom.p.x = 0_f;
                v.geom.p.z = 0_f;

                subpathL.vertices.push_back(v);
            }
            
            // 2
            {
                const auto& pv = subpathL.vertices.back();
                Ray ray = { pv.geom.p, Vec3(0_f, -1_f, 0_f) };
                Intersection isect;
                if (!scene->Intersect(ray, isect)) { LM_UNREACHABLE(); return; }

                SubpathSampler::PathVertex v;
                v.geom = isect.geom;
                v.primitive = isect.primitive;
                v.type = isect.primitive->Type() & ~SurfaceInteractionType::Emitter;
                subpathL.vertices.push_back(v);
            }

            // 3
            {
                const auto& pv = subpathL.vertices.back();
                const auto& ppv = subpathL.vertices[subpathL.vertices.size()-2];

                Ray ray;
                ray.o = pv.geom.p;
                pv.primitive->SampleDirection(Vec2(), 0_f, pv.type, pv.geom, Math::Normalize(pv.geom.p - ppv.geom.p), ray.d);
                Intersection isect;
                if (!scene->Intersect(ray, isect)) { LM_UNREACHABLE(); return; }

                SubpathSampler::PathVertex v;
                v.geom = isect.geom;
                v.primitive = isect.primitive;
                v.type = isect.primitive->Type() & ~SurfaceInteractionType::Emitter;
                subpathL.vertices.push_back(v);
            }
        }

        // Get uv coordinates of the intersected point
        

        // For each points on intersected quad
        const int BinSize = 10;
        std::vector<Float> dist(BinSize*BinSize);
        for (int i = 0; i < BinSize; i++)
        {
            for (int j = 0; j < BinSize; j++)
            {
                const auto D = 1_f / BinSize;
                Vec3 p;
                p.x = (((Float)j + 0.5_f) / BinSize) * 2_f - 1_f;
                p.y = subpathL.vertices.back().geom.p.y;
                p.z = (((Float)i + 0.5_f) / BinSize) * 2_f - 1_f;

                // Run ManifoldWalk for the new point p

                // Record the result
                dist[i*BinSize + j] = Math::Length(subpathL.vertices.back().geom.p - p);
            }
        }

        // Record data
        {
            const auto path = outputPath + ".dat";
            LM_LOG_INFO("Saving output: " + path);
            const auto parent = boost::filesystem::path(path).parent_path();
            if (!boost::filesystem::exists(parent) && parent != "") { boost::filesystem::create_directories(parent); }
            std::ofstream ofs(path, std::ios::out | std::ios::binary);
            // Bin size
            ofs.write((const char*)&BinSize, sizeof(int));
            ofs.write((const char*)dist.data(), sizeof(Float) * BinSize * BinSize);
        }
    };

};

LM_COMPONENT_REGISTER_IMPL(Renderer_Debug_ManifoldWalk, "renderer::invmap_debug_manifoldwalk");

LM_NAMESPACE_END
