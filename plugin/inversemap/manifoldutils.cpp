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

#include "manifoldutils.h"
#include "debugio.h"
#include <cereal/archives/json.hpp>
#include <cereal/types/vector.hpp>

#if LM_COMPILER_MSVC
#pragma warning(disable:4714)
#pragma warning(disable:4701)
#pragma warning(disable:4456)
#include <Eigen/Dense>
#else
#include <eigen3/Eigen/Dense>
#endif

#define INVERSEMAP_MANIFOLDWALK_USE_EIGEN_SOLVER 1
#define INVERSEMAP_MANIFOLDWALK_BETA_EXT 0

LM_NAMESPACE_BEGIN

using Matrix = Eigen::Matrix<Float, Eigen::Dynamic, Eigen::Dynamic>;
using Vector = Eigen::Matrix<Float, Eigen::Dynamic, 1>;

template <class Archive>
auto serialize(Archive& archive, Vec3& v) -> void
{
    archive(cereal::make_nvp("x", v.x), cereal::make_nvp("y", v.y), cereal::make_nvp("z", v.z));
}


namespace
{

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

}

#if 0
auto ManifoldUtils::ComputeConstraintJacobian(const Subpath& path, ConstraintJacobian& nablaC) -> void
{
	const int n = (int)(path.vertices.size());
	for (int i = 1; i < n - 1; i++)
	{
		#pragma region Some precomputation

        const auto vi  = path.vertices[i];
        const auto vip = path.vertices[i - 1];
        const auto vin = path.vertices[i + 1];

		const auto& x  = vi.geom;
		const auto& xp = vip.geom;
		const auto& xn = vin.geom;
			
		const auto wi  = Math::Normalize(xp.p - x.p);
		const auto wo  = Math::Normalize(xn.p - x.p);
        const auto eta = 1_f / vi.primitive->bsdf->Eta(x, wi);
		const auto H   = Math::Normalize(wi + eta * wo);

		const auto inv_wiL = 1_f / Math::Length(xp.p - x.p);     // ili
		const auto inv_woL = 1_f / Math::Length(xn.p - x.p);     // ilo
		const auto inv_HL  = 1_f / Math::Length(wi + eta * wo);  // ilh
			
		const auto dot_H_n    = Math::Dot(x.sn, H);
		const auto dot_H_dndu = Math::Dot(x.dndu, H);
		const auto dot_H_dndv = Math::Dot(x.dndv, H);
		const auto dot_u_n    = Math::Dot(x.dpdu, x.sn);
		const auto dot_v_n    = Math::Dot(x.dpdv, x.sn);

		const auto s = x.dpdu - dot_u_n * x.sn;
		const auto t = x.dpdv - dot_v_n * x.sn;

		const auto div_inv_wiL_HL = inv_wiL * inv_HL;         // ili := ili * ilh
		const auto div_inv_woL_HL = inv_woL * inv_HL * eta;   // ilo := ilo * eta * ilh

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
#else
auto ManifoldUtils::ComputeConstraintJacobian(const Subpath& path, ConstraintJacobian& nablaC) -> void
{
	const int n = (int)(path.vertices.size());
	for (int i = 1; i < n - 1; i++)
	{
		#pragma region Some precomputation

        const auto vi  = path.vertices[i];
        const auto vip = path.vertices[i - 1];
        const auto vin = path.vertices[i + 1];

		const auto& x  = vi.geom;
		const auto& xp = vip.geom;
		const auto& xn = vin.geom;
			
		const auto wi  = Math::Normalize(xp.p - x.p);
		const auto wo  = Math::Normalize(xn.p - x.p);
        const auto eta = 1_f / vi.primitive->bsdf->Eta(x, wi);
        // No need to normalize H for index-matched materials or reflections
        const bool normalizeH = eta != 1_f;
		const auto H   = normalizeH ? Math::Normalize(wi + eta * wo) : wi + wo;

		const auto inv_wiL = 1_f / Math::Length(xp.p - x.p);                        // ili
		const auto inv_woL = 1_f / Math::Length(xn.p - x.p);                        // ilo
        const auto inv_HL = normalizeH ? 1_f / Math::Length(wi + eta * wo) : 1_f;  // ilh
			
		const auto dot_H_n    = Math::Dot(x.sn, H);
		const auto dot_H_dndu = Math::Dot(x.dndu, H);
		const auto dot_H_dndv = Math::Dot(x.dndv, H);
		const auto dot_u_n    = Math::Dot(x.dpdu, x.sn);
		const auto dot_v_n    = Math::Dot(x.dpdv, x.sn);

		const auto s = x.dpdu - dot_u_n * x.sn;
		const auto t = x.dpdv - dot_v_n * x.sn;

		const auto div_inv_wiL_HL = inv_wiL * inv_HL;         // ili := ili * ilh
		const auto div_inv_woL_HL = inv_woL * inv_HL * eta;   // ilo := ilo * eta * ilh

		#pragma endregion

		// --------------------------------------------------------------------------------

		#pragma region Compute A_i (derivative w.r.t. x_{i-1})
			
		{
			const auto tu = (xp.dpdu - wi * Math::Dot(wi, xp.dpdu)) * div_inv_wiL_HL;
			const auto tv = (xp.dpdv - wi * Math::Dot(wi, xp.dpdv)) * div_inv_wiL_HL;
			const auto dHdu = normalizeH ? tu - H * Math::Dot(tu, H) : tu;
			const auto dHdv = normalizeH ? tv - H * Math::Dot(tv, H) : tv;
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
			const auto dHdu = normalizeH ? tu - H * Math::Dot(tu, H) : tu;
			const auto dHdv = normalizeH ? tv - H * Math::Dot(tv, H) : tv;
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
			const auto dHdu = normalizeH ? tu - H * Math::Dot(tu, H) : tu;
			const auto dHdv = normalizeH ? tv - H * Math::Dot(tv, H) : tv;
			nablaC[i - 1].C = Mat2(
				Math::Dot(dHdu, s), Math::Dot(dHdu, t),
				Math::Dot(dHdv, s), Math::Dot(dHdv, t));
		}

		#pragma endregion
	}
}
#endif

// --------------------------------------------------------------------------------

namespace
{
    template <typename t_matrix>
    t_matrix PseudoInverse(const t_matrix& m, const double &tolerance = 1.e-6)
    {
        using namespace Eigen;
        typedef JacobiSVD<t_matrix> TSVD;
        unsigned int svd_opt(ComputeThinU | ComputeThinV);
        if (m.RowsAtCompileTime != Dynamic || m.ColsAtCompileTime != Dynamic)
            svd_opt = ComputeFullU | ComputeFullV;
        TSVD svd(m, svd_opt);
        const typename TSVD::SingularValuesType &sigma(svd.singularValues());
        typename TSVD::SingularValuesType sigma_inv(sigma.size());
        for (long i = 0; i<sigma.size(); ++i)
        {
            if (sigma(i) > tolerance)
                sigma_inv(i) = 1.0 / sigma(i);
            else
                sigma_inv(i) = 0.0;
        }
        return svd.matrixV()*sigma_inv.asDiagonal()*svd.matrixU().transpose();
    }
}

auto ManifoldUtils::ComputeConstraintJacobianDeterminant(const Subpath& subpath) -> Float
{
    const int n = (int)(subpath.vertices.size());

    // --------------------------------------------------------------------------------

    ConstraintJacobian nablaC(n - 2);
    ComputeConstraintJacobian(subpath, nablaC);

    // --------------------------------------------------------------------------------

    // A
    Matrix A = Matrix::Zero(2 * (n - 2), 2 * (n - 2));
    for (int i = 0; i < n - 2; i++)
    {
        if (i > 0)
        {
            const auto& A_ = nablaC[i].A;
            Eigen::Array22d m;
            m << A_[0][0], A_[1][0],
                 A_[0][1], A_[1][1];
            A.block<2, 2>(i * 2, (i - 1) * 2) = m;
        }
        {
            const auto& B_ = nablaC[i].B;
            Eigen::Array22d m;
            m << B_[0][0], B_[1][0],
                 B_[0][1], B_[1][1];
            A.block<2, 2>(i * 2, i * 2) = m;
        }
        if (i < n - 2 - 1)
        {
            const auto& C_ = nablaC[i].C;
            Eigen::Array22d m;
            m << C_[0][0], C_[1][0],
                 C_[0][1], C_[1][1];
            A.block<2, 2>(i * 2, (i + 1) * 2) = m;
        }
    }

    // A^-1
    const decltype(A) invA = A.inverse();
    //const decltype(A) invA = PseudoInverse(A);

    // P_2 A^-1 B_n
    const auto Bn_np   = nablaC[n - 3].C;
    const auto invA_0n = Mat2(invA(0, 2*(n-3)), invA(1, 2*(n-3)), invA(0, 2*(n-3)+1), invA(1, 2*(n-3)+1));
    const auto invA_Bn = invA_0n * Bn_np;
    const auto det     = invA_Bn[0][0] * invA_Bn[1][1] - invA_Bn[1][0] * invA_Bn[0][1];

    // --------------------------------------------------------------------------------
    return Math::Abs(det);
}

// Returns the converged path. Returns none if not converged.
auto ManifoldUtils::WalkManifold(const Scene* scene, const Subpath& seedPath, const Vec3& target) -> boost::optional<Subpath>
{
	#pragma region Preprocess

	// Number of path vertices
	const int n = (int)(seedPath.vertices.size());

	// Initial path
    Subpath currP = seedPath;

	#pragma endregion

    // --------------------------------------------------------------------------------

    #if INVERSEMAP_MANIFOLDWALK_DEBUG_IO
    LM_LOG_DEBUG("seed_path");
    {
        DebugIO::Wait();
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
        DebugIO::Output("seed_path", ss.str());
    }       
    #endif

	// --------------------------------------------------------------------------------

    #if INVERSEMAP_MANIFOLDWALK_DEBUG_IO
    LM_LOG_DEBUG("target");
    {
        DebugIO::Wait();
        std::vector<double> vs;
        for (int i = 0; i < 3; i++) vs.push_back(target[i]);
        std::stringstream ss;
        {
            cereal::JSONOutputArchive oa(ss);
            oa(vs);
        }
        DebugIO::Output("target", ss.str());
    }
    #endif

    // --------------------------------------------------------------------------------

    //#if INVERSEMAP_MANIFOLDWALK_DEBUG_IO
    //LM_LOG_DEBUG("tanget_frame_v1");
    //{
    //    DebugIO::Wait();
    //    std::stringstream ss;
    //    {
    //        cereal::JSONOutputArchive oa(ss);
    //        const auto& v = currP.vertices[1];
    //        oa(v.geom.p, v.geom.sn, v.geom.dpdu, v.geom.dpdv);
    //    }
    //    DebugIO::Output("tanget_frame_v1", ss.str());
    //}
    //#endif

    // --------------------------------------------------------------------------------

	#pragma region Optimization loop

	const Float MaxBeta = 100.0;
    #if INVERSEMAP_MANIFOLDWALK_BETA_EXT
    Vec2 beta(MaxBeta, MaxBeta);
    #else
	Float beta = MaxBeta;
    #endif
	const Float Eps = 1e-4;
	const int MaxIter = 50;

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
            DebugIO::Wait();
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
            DebugIO::Output("current_path", ss.str());
        }       
        #endif

        // --------------------------------------------------------------------------------

        //#if INVERSEMAP_MANIFOLDWALK_DEBUG_IO
        //LM_LOG_DEBUG("current_tanget_frame_v1");
        //{
        //    DebugIO::Wait();
        //    std::stringstream ss;
        //    {
        //        cereal::JSONOutputArchive oa(ss);
        //        const auto& v = currP.vertices[1];
        //        oa(v.geom.p, v.geom.sn, v.geom.dpdu, v.geom.dpdv);
        //    }
        //    DebugIO::Output("current_tanget_frame_v1", ss.str());
        //}
        //#endif

        // --------------------------------------------------------------------------------

        {
            //const auto d = Math::Length(currP.vertices.back().geom.p - target);
            //LM_LOG_DEBUG(boost::str(boost::format("#%02d: Dist to target %.15f") % iteration % d));
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
            #if !INVERSEMAP_MANIFOLDWALK_USE_EIGEN_SOLVER
			SolveBlockLinearEq(nablaC, V, W);
            #else
            Matrix nablaC_ = Matrix::Zero(2*(n-2),2*(n-2));
            for (int i = 0; i < n - 2; i++)
            {
                if (i > 0)
                {
                    const auto& A = nablaC[i].A;
                    Eigen::Array22d m;
                    m << A[0][0], A[1][0],
                         A[0][1], A[1][1];
                    nablaC_.block<2, 2>(i * 2, (i - 1) * 2) = m;
                }
                {
                    const auto& B = nablaC[i].B;
                    Eigen::Array22d m;
                    m << B[0][0], B[1][0],
                         B[0][1], B[1][1];
                    nablaC_.block<2, 2>(i * 2, i * 2) = m;
                }
                if (i < n - 2 - 1)
                {
                    const auto& C = nablaC[i].C;
                    Eigen::Array22d m;
                    m << C[0][0], C[1][0],
                         C[0][1], C[1][1];
                    nablaC_.block<2, 2>(i * 2, (i + 1) * 2) = m;

                }
            }
            Vector V_(2*(n - 2));
            for (int i = 0; i < n - 2; i++) { V_(2*i) = V[i].x; V_(2*i+1) = V[i].y; }
            Vector W_ = nablaC_.colPivHouseholderQr().solve(V_);
            for (int i = 0; i < n - 2; i++) { W[i].x = W_(2 * i); W[i].y = W_(2 * i + 1); }
            #endif

			// x_2, T(x_2)
			const auto& x2 = currP.vertices[1].geom.p;
			const Mat3x2 Tx2(currP.vertices[1].geom.dpdu, currP.vertices[1].geom.dpdv);

            #if INVERSEMAP_MANIFOLDWALK_DEBUG_IO
            LM_LOG_DEBUG("points_on_tangent_s");
            {
                DebugIO::Wait();
                std::vector<Vec3> vs;
                for (int i = 0; i < n - 2; i++)
                {
                    const Mat3x2 Tx(currP.vertices[i + 1].geom.dpdu, currP.vertices[i + 1].geom.dpdv);
                    vs.push_back(currP.vertices[i + 1].geom.p +  Tx * W[i]);
                }
                std::stringstream ss;
                {
                    cereal::JSONOutputArchive oa(ss);
                    oa(vs);
                }
                DebugIO::Output("points_on_tangent_s", ss.str());
            }
            #endif

			// W_{n-2} = P_2 W
			//const auto Wn2p = W[n - 3];
            const auto Wn2p = W[0];

			// p = x_2 - \beta T(x_2) P_2 W_{n-2}
            #if INVERSEMAP_MANIFOLDWALK_BETA_EXT
            const auto t1 = Vec2(Wn2p.x * beta.x, Wn2p.y * beta.y);
            const auto t2 = Tx2 * t1;
            #else
            const auto t1 = Tx2 * Wn2p;
            const auto t2 = t1 * beta;
            #endif
			return x2 - t2;
        }();
		#pragma endregion

        // --------------------------------------------------------------------------------

        #if INVERSEMAP_MANIFOLDWALK_DEBUG_IO
        LM_LOG_DEBUG("point_on_tangent");
        {
            DebugIO::Wait();
            std::vector<double> vs;
            for (int i = 0; i < 3; i++) vs.push_back(p[i]);
            std::stringstream ss;
            {
                cereal::JSONOutputArchive oa(ss);
                oa(vs);
            }
            DebugIO::Output("point_on_tangent", ss.str());
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
                        const auto uC = [&]() -> Float
                        {
                            // Fix sampled component for Flesnel material (TODO. refactor it)
                            // Vertices in current path
                            const auto& curr_v  = currP.vertices[i - 1];
                            const auto& curr_vp = currP.vertices[i - 2];
                            const auto& curr_vn = currP.vertices[i];
                            const auto wo = Math::Normalize(curr_vn.geom.p - curr_v.geom.p);
                            const auto wi = Math::Normalize(curr_vp.geom.p - curr_v.geom.p);
                            const auto localWo = curr_v.geom.ToLocal * wo;
                            const auto localWi = curr_v.geom.ToLocal * wi;
                            return Math::LocalCos(localWi) * Math::LocalCos(localWo) >= 0_f ? 0_f : 1_f;
                        }();
                        Vec3 wo;
                        vp->primitive->SampleDirection(Vec2(), uC, vp->type, vp->geom, Math::Normalize(vpp->geom.p - vp->geom.p), wo);
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
                    
                // Fails if the last vertex is S
                if (i == n - 1 && (isect.primitive->Type() & SurfaceInteractionType::S) != 0)
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
            DebugIO::Wait();
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
            DebugIO::Output("next_path", ss.str());
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
            #if INVERSEMAP_MANIFOLDWALK_BETA_EXT
            if (Math::Abs(beta.x) > Math::Abs(beta.y)) { beta.x *= -0.5_f; }
            else { beta.y *= -0.5_f; }
            #else
            beta *= -0.5_f;
            #endif
            //LM_LOG_INFO(boost::str(boost::format("- beta: %.15f") % beta));
        }
        else
        {
            #if INVERSEMAP_MANIFOLDWALK_BETA_EXT
            beta.x = Math::Clamp(beta.x * 2_f, -MaxBeta, MaxBeta);
            beta.y = Math::Clamp(beta.y * 2_f, -MaxBeta, MaxBeta);
            //if (Math::Abs(beta.x) > Math::Abs(beta.y)) { beta.y = Math::Clamp(beta.y * 2_f, -MaxBeta, MaxBeta); }
            //else { beta.x = Math::Clamp(beta.x * 2_f, -MaxBeta, MaxBeta); }
            #else
            beta = Math::Clamp(beta * 2_f, -MaxBeta, MaxBeta);
            #endif
            //LM_LOG_INFO(boost::str(boost::format("+ beta: %.15f") % beta));
            currP = *nextP;
        }
		#pragma endregion
	}

	#pragma endregion

	// --------------------------------------------------------------------------------
	return boost::none;
}

LM_NAMESPACE_END
