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
#include "debugio.h"
#include <chrono>
#include <thread>
#include <cereal/archives/json.hpp>
#include <cereal/types/vector.hpp>

#define INVERSEMAP_MANIFOLDWALK_OUTPUT_TRIANGLES          0
#define INVERSEMAP_MANIFOLDWALK_OUTPUT_FAILED_TRIAL_PATHS 0
#define INVERSEMAP_MANIFOLDWALK_SINGLE_TARGET             1
#define INVERSEMAP_MANIFOLDWALK_DEBUG_IO                  1

LM_NAMESPACE_BEGIN

namespace
{
    DebugIO io_;
}

template <class Archive>
auto serialize(Archive& archive, Vec3& v) -> void
{
    archive(cereal::make_nvp("x", v.x), cereal::make_nvp("y", v.y), cereal::make_nvp("z", v.z));
}

namespace
{
    struct VertexConstraintJacobian
    {
        Mat2 A;
        Mat2 B;
        Mat2 C;
    };

    using ConstraintJacobian = std::vector<VertexConstraintJacobian>;

	auto ComputeConstraintJacobian(const Subpath& path, ConstraintJacobian& nablaC) -> void
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

			#pragma region Compute A_i (derivative w.r.t. x_{i-1})
			
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

			#pragma region Compute B_i (derivative w.r.t. x_i)

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

			#pragma region Compute C_i (derivative w.r.t. x_{i+1})

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
			// U_1 = A'_1
			U[0] = nablaC[0].B;
			for (int i = 1; i < n; i++)
			{
				L[i] = nablaC[i].A * Math::Inverse(U[i-1]);		// L_i = C'_i U_{i-1}^-1
				U[i] = nablaC[i].B - L[i] * nablaC[i-1].C;		// U_i = A'_i - L_i * B'_{i-1}
			}
		}

		#pragma endregion

		// --------------------------------------------------------------------------------

		#pragma region Forward substitution
 
		// Solve L V' = V
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

		// Solve U_n W_n = V'_n
		W[n - 1] = Math::Inverse(U[n - 1]) * Vp[n - 1];

		for (int i = n - 2; i >= 0; i--)
		{
			// Solve U_i W_i = V'_i - V_i W_{i+1}
			W[i] = Math::Inverse(U[i]) * (Vp[i] - V[i] * W[i + 1]);
		}

		#pragma endregion
	}

    // Returns the converged path. Returns none if not converged.
	auto WalkManifold(const Scene* scene, const Subpath& seedPath, const Vec3& target) -> boost::optional<Subpath>
	{
		#pragma region Preprocess

		// Number of path vertices
		const int n = (int)(seedPath.vertices.size());

		// Initial path
        Subpath currP = seedPath;

		//// Compute \nabla C
		//ConstraintJacobian nablaC(n - 2);
		//ComputeConstraintJacobian(currP, nablaC);

		//// Compute L
		//Float L = 0;
		//for (const auto& x : currP.vertices) { L = Math::Max(L, Math::Length(x.geom.p)); }

		#pragma endregion

        // --------------------------------------------------------------------------------

        #if INVERSEMAP_MANIFOLDWALK_DEBUG_IO
        LM_LOG_DEBUG("seed_path");
        {
            io_.Wait();
            std::vector<double> vs;
            for (const auto& v : currP.vertices)
            {
                for (int i = 0; i < 3; i++) vs.push_back(v.geom.p[i]);
            }
            std::stringstream ss;
            {
                cereal::JSONOutputArchive oa(ss);
                oa(vs);
            }
            io_.Output("seed_path", ss.str());
        }       
        #endif

		// --------------------------------------------------------------------------------

        #if INVERSEMAP_MANIFOLDWALK_DEBUG_IO
        LM_LOG_DEBUG("target");
        {
            io_.Wait();
            std::vector<double> vs;
            for (int i = 0; i < 3; i++) vs.push_back(target[i]);
            std::stringstream ss;
            {
                cereal::JSONOutputArchive oa(ss);
                oa(vs);
            }
            io_.Output("target", ss.str());
        }
        #endif

        // --------------------------------------------------------------------------------

        //#if INVERSEMAP_MANIFOLDWALK_DEBUG_IO
        //LM_LOG_DEBUG("tanget_frame_v1");
        //{
        //    io_.Wait();
        //    std::stringstream ss;
        //    {
        //        cereal::JSONOutputArchive oa(ss);
        //        const auto& v = currP.vertices[1];
        //        oa(v.geom.p, v.geom.sn, v.geom.dpdu, v.geom.dpdv);
        //    }
        //    io_.Output("tanget_frame_v1", ss.str());
        //}
        //#endif

        // --------------------------------------------------------------------------------

		#pragma region Optimization loop

		const Float MaxBeta = 100.0;
		Float beta = MaxBeta;
		const Float Eps = 1e-5;
		const int MaxIter = 30;

		for (int iteration = 0; iteration < MaxIter; iteration++)
		{
            #if INVERSEMAP_MANIFOLDWALK_OUTPUT_FAILED_TRIAL_PATHS
            {
                static long long count = 0;
                if (count == 0)
                {
                    boost::filesystem::remove("dirs.out");
                }
                {
                    count++;
                    std::ofstream out("dirs.out", std::ios::out | std::ios::app);
                    for (const auto& v : currP.vertices)
                    {
                        out << boost::str(boost::format("%.10f %.10f %.10f ") % v.geom.p.x % v.geom.p.y % v.geom.p.z);
                    }
                    out << std::endl;
                }
            }
            #endif

            // --------------------------------------------------------------------------------

            #if INVERSEMAP_MANIFOLDWALK_DEBUG_IO
            LM_LOG_DEBUG("current_path");
            {
                io_.Wait();
                std::vector<double> vs;
                for (const auto& v : currP.vertices)
                {
                    for (int i = 0; i < 3; i++) vs.push_back(v.geom.p[i]);
                }
                std::stringstream ss;
                {
                    cereal::JSONOutputArchive oa(ss);
                    oa(vs);
                }
                io_.Output("current_path", ss.str());
            }       
            #endif

            // --------------------------------------------------------------------------------

            #if INVERSEMAP_MANIFOLDWALK_DEBUG_IO
            LM_LOG_DEBUG("current_tanget_frame_v1");
            {
                io_.Wait();
                std::stringstream ss;
                {
                    cereal::JSONOutputArchive oa(ss);
                    const auto& v = currP.vertices[1];
                    oa(v.geom.p, v.geom.sn, v.geom.dpdu, v.geom.dpdv);
                }
                io_.Output("current_tanget_frame_v1", ss.str());
            }
            #endif

            // --------------------------------------------------------------------------------

            {
                const auto d = Math::Length(currP.vertices.back().geom.p - target);
                //LM_LOG_INFO(boost::str(boost::format("#%02d: Dist to target %.15f") % iteration % d));
            }

            // --------------------------------------------------------------------------------

            // Compute \nabla C
            ConstraintJacobian nablaC(n - 2);
            ComputeConstraintJacobian(currP, nablaC);

            // Compute L
            Float L = 0;
            for (const auto& x : currP.vertices) { L = Math::Max(L, Math::Length(x.geom.p)); }

            // --------------------------------------------------------------------------------

			#pragma region Stop condition
			if (Math::Length(currP.vertices[n - 1].geom.p - target) < Eps * L)
			{
                //LM_LOG_INFO("Converged");
                return currP;
			}
			#pragma endregion

			// --------------------------------------------------------------------------------

			#pragma region Compute movement in tangement plane
			// New position of initial specular vertex
			const auto p = [&]() -> Vec3
			{
				// x_n, x'_n
				const auto& xn = currP.vertices[n - 1].geom.p;
				const auto& xnp = target;

				// T(x_n)^T
                const auto Txn = Mat3x2(currP.vertices[n - 1].geom.dpdu, currP.vertices[n - 1].geom.dpdv);
				const auto TxnT = Math::Transpose(Txn);

				// V \equiv B_n T(x_n)^T (x'_n - x)
				const auto Bn_n2p = nablaC[n - 3].C;
				const auto V_n2p = Bn_n2p * TxnT * (xnp - xn);

				// Solve AW = V
				std::vector<Vec2> V(n - 2);
				std::vector<Vec2> W(n - 2);
				for (int i = 0; i < n - 2; i++) { V[i] = i == n - 3 ? V_n2p : Vec2(); }
				SolveBlockLinearEq(nablaC, V, W);

				// x_2, T(x_2)
				const auto& x2 = currP.vertices[1].geom.p;
				const Mat3x2 Tx2(currP.vertices[1].geom.dpdu, currP.vertices[1].geom.dpdv);

				// W_{n-2} = P_2 W
				const auto Wn2p = W[n - 3];
                //const auto Wn2p = W[0];

				// p = x_2 - \beta T(x_2) P_2 W_{n-2}
				return x2 - Tx2 * Wn2p * beta;
            }();
			#pragma endregion

            // --------------------------------------------------------------------------------

            #if INVERSEMAP_MANIFOLDWALK_DEBUG_IO
            LM_LOG_DEBUG("point_on_tangent");
            {
                io_.Wait();
                std::vector<double> vs;
                for (int i = 0; i < 3; i++) vs.push_back(p[i]);
                std::stringstream ss;
                {
                    cereal::JSONOutputArchive oa(ss);
                    oa(vs);
                }
                io_.Output("point_on_tangent", ss.str());
            }
            #endif

			// --------------------------------------------------------------------------------

			#pragma region Propagate light path to p - x1
            const auto nextP = [&]() -> boost::optional<Subpath>
            {
                Subpath nextP;
                nextP.vertices.push_back(currP.vertices[0]);
                for (int i = 1; i < n; i++)
                {
                    // Current vertex & previous vertex
                    const auto* vp = &nextP.vertices[i - 1];
                    const auto* vpp = i - 2 >= 0 ? &nextP.vertices[i - 2] : nullptr;

                    // Next ray direction
                    const auto wo = [&]()
                    {
                        if (i == 1) { return Math::Normalize(p - vp->geom.p); }
                        else
                        {
                            assert(vp->type == SurfaceInteractionType::S);
                            Vec3 wo;
                            vp->primitive->SampleDirection(Vec2(), 0_f, vp->type, vp->geom, Math::Normalize(vpp->geom.p - vp->geom.p), wo);
                            return wo;
                        }
                    }();

                    // Intersection query
                    Ray ray = { vp->geom.p, wo };
                    Intersection isect;
                    if (!scene->Intersect(ray, isect))
                    {
                        return boost::none;
                    }

                    // Fails if not intersected with specular vertex, except for the last vertex.
                    if (i <= n - 2 && (isect.primitive->Type() & SurfaceInteractionType::S) == 0)
                    {
                        return boost::none;
                    }
                    
                    // Fails if the last vertex is not D
                    if (i == n - 1 && (isect.primitive->Type() & SurfaceInteractionType::D) == 0)
                    {
                        return boost::none;
                    }

                    // Add vertex
                    SubpathSampler::PathVertex v;
                    v.geom = isect.geom;
                    v.primitive = isect.primitive;
                    v.type = isect.primitive->Type() & ~SurfaceInteractionType::Emitter;
                    nextP.vertices.push_back(v);
                }
                return nextP;
            }();
			#pragma endregion

            // --------------------------------------------------------------------------------
            
            #if INVERSEMAP_MANIFOLDWALK_DEBUG_IO
            if (nextP)
            {
                LM_LOG_DEBUG("next_path");
                io_.Wait();
                std::vector<double> vs;
                for (const auto& v : nextP->vertices)
                {
                    for (int i = 0; i < 3; i++) vs.push_back(v.geom.p[i]);
                }
                std::stringstream ss;
                {
                    cereal::JSONOutputArchive oa(ss);
                    oa(vs);
                }
                io_.Output("next_path", ss.str());
            }
            #endif

            // --------------------------------------------------------------------------------

            #if INVERSEMAP_MANIFOLDWALK_OUTPUT_FAILED_TRIAL_PATHS
            if (nextP)
            {
                static long long count = 0;
                if (count == 0)
                {
                    boost::filesystem::remove("dirs_next.out");
                }
                {
                    count++;
                    std::ofstream out("dirs_next.out", std::ios::out | std::ios::app);
                    for (const auto& v : nextP->vertices)
                    {
                        out << boost::str(boost::format("%.10f %.10f %.10f ") % v.geom.p.x % v.geom.p.y % v.geom.p.z);
                    }
                    out << std::endl;
                }
            }
            #endif

			// --------------------------------------------------------------------------------

			#pragma region Update beta
            const auto update = [&]() -> bool
            {
                if (!nextP)
                {
                    return true;
                }
                // Update beta if nextP shows larger difference to target
                const auto d  = Math::Length2(currP.vertices.back().geom.p - target);
                const auto dn = Math::Length2(nextP->vertices.back().geom.p - target);
                //LM_LOG_INFO(boost::str(boost::format("d, dn: %.15f %.15f") % d % dn));
                if (dn >= d)
                {
                    return true;
                }
                return false;
            }();
            if (update)
            {
                beta *= 0.5_f;
                //LM_LOG_INFO(boost::str(boost::format("- beta: %.15f") % beta));
            }
            else
            {
                beta = Math::Min(MaxBeta, beta * 1.7_f);
                //LM_LOG_INFO(boost::str(boost::format("+ beta: %.15f") % beta));
                currP = *nextP;
            }
			#pragma endregion
		}

		#pragma endregion

		// --------------------------------------------------------------------------------
		return boost::none;
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
        io_.Run();

        struct Test
        {
            int x, y, z;
            template<class Archive>
            void serialize(Archive & archive)
            {
                archive(x, y, z);
            }
        };

        //while (io_.Wait())
        //{
        //    // Input and serialize
        //    Test t;
        //    {
        //        std::stringstream ss(io_.Input());
        //        cereal::JSONInputArchive ia(ss);
        //        ia(t);
        //        LM_LOG_DEBUG("In");
        //        LM_LOG_DEBUG(ss.str());
        //    }

        //    t.x *= 2;
        //    t.y *= 2;
        //    t.z *= 2;

        //    // Super long loop
        //    std::this_thread::sleep_for(std::chrono::seconds(5));

        //    std::stringstream ss;
        //    {
        //        cereal::JSONOutputArchive oa(ss);
        //        oa(t);
        //    }
        //    LM_LOG_DEBUG("Out");
        //    LM_LOG_DEBUG(ss.str());
        //    io_.Output(ss.str());
        //}

        // --------------------------------------------------------------------------------

        #if INVERSEMAP_MANIFOLDWALK_OUTPUT_TRIANGLES
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

        #if INVERSEMAP_MANIFOLDWALK_DEBUG_IO
        LM_LOG_DEBUG("triangle_vertices");
        {
            io_.Wait();

            std::vector<double> vs;
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
                    for (int j = 0; j < 3; j++) vs.push_back(p1[j]);
                    for (int j = 0; j < 3; j++) vs.push_back(p2[j]);
                    for (int j = 0; j < 3; j++) vs.push_back(p3[j]);
                }
            }
            
            std::stringstream ss;
            {
                cereal::JSONOutputArchive oa(ss);
                oa(vs);
            }

            io_.Output("triangle_vertices", ss.str());
        }
        #endif


        // --------------------------------------------------------------------------------

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
            for (int i = 0; i < 2; i++)
            {
                const auto& pv = subpathL.vertices.back();
                const auto& ppv = subpathL.vertices[subpathL.vertices.size()-2];

                Ray ray;
                ray.o = pv.geom.p;
                pv.primitive->SampleDirection(Vec2(), 0_f, pv.type, pv.geom, Math::Normalize(ppv.geom.p - pv.geom.p), ray.d);
                Intersection isect;
                if (!scene->Intersect(ray, isect)) { LM_UNREACHABLE(); return; }

                SubpathSampler::PathVertex v;
                v.geom = isect.geom;
                v.primitive = isect.primitive;
                v.type = isect.primitive->Type() & ~SurfaceInteractionType::Emitter;
                subpathL.vertices.push_back(v);
            }
        }

        // --------------------------------------------------------------------------------

        #if INVERSEMAP_MANIFOLDWALK_OUTPUT_FAILED_TRIAL_PATHS
        {
            static long long count = 0;
            if (count == 0)
            {
                boost::filesystem::remove("dirs_orig.out");
            }
            {
                count++;
                std::ofstream out("dirs_orig.out", std::ios::out | std::ios::app);
                for (const auto& v : subpathL.vertices)
                {
                    out << boost::str(boost::format("%.10f %.10f %.10f ") % v.geom.p.x % v.geom.p.y % v.geom.p.z);
                }
                out << std::endl;
            }
        }
        #endif

        // --------------------------------------------------------------------------------

        // For each points on intersected quad
        const int BinSize = 100;
        std::vector<Float> dist(BinSize*BinSize);
        #if INVERSEMAP_MANIFOLDWALK_SINGLE_TARGET

        #if INVERSEMAP_MANIFOLDWALK_DEBUG_IO
        int I;
        LM_LOG_DEBUG("waiting_for_input");
        {
            io_.Wait();
            std::stringstream ss(io_.Input());
            {
                cereal::JSONInputArchive ia(ss);
                ia(cereal::make_nvp("selected_target_id", I));
            }
        }
        #else
        const int I = 44;
        #endif
        
        for (int i = I; i <= I; i++)
        #else
        for (int i = 0; i < BinSize; i++)
        #endif
        {
            #if INVERSEMAP_MANIFOLDWALK_SINGLE_TARGET
            for (int j = I; j <= I; j++)
            #else
            for (int j = 0; j < BinSize; j++)
            #endif
            {
                const auto D = 1_f / BinSize;
                Vec3 p;
                p.x = (((Float)j + 0.5_f) / BinSize) * 2_f - 1_f;
                p.y = -1_f;
                p.z = (((Float)i + 0.5_f) / BinSize) * 2_f - 1_f;

                #if INVERSEMAP_MANIFOLDWALK_OUTPUT_FAILED_TRIAL_PATHS
                {
                    static long long count = 0;
                    if (count == 0)
                    {
                        boost::filesystem::remove("targets.out");
                    }
                    {
                        count++;
                        std::ofstream out("targets.out", std::ios::out | std::ios::app);
                        out << boost::str(boost::format("%.10f %.10f %.10f ") % p.x % p.y % p.z);
                        out << std::endl;
                    }
                }
                #endif

                // Run ManifoldWalk for the new point p
                const auto connPath    = WalkManifold(scene, subpathL, p);
                if (!connPath)
                {
                    dist[i*BinSize + j] = 0_f;
                    continue;
                }

                const auto connPathInv = WalkManifold(scene, *connPath, subpathL.vertices.back().geom.p);
                if (!connPathInv)
                {
                    dist[i*BinSize + j] = 0.5_f;
                    continue;
                }

                dist[i*BinSize + j] = 1_f;
            }
        }

        // --------------------------------------------------------------------------------

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

        // --------------------------------------------------------------------------------

        io_.Stop();
    };

};

LM_COMPONENT_REGISTER_IMPL(Renderer_Debug_ManifoldWalk, "renderer::invmap_debug_manifoldwalk");

LM_NAMESPACE_END
