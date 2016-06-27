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

#define INVERSEMAP_MLTINVMAPFIXED_DEBUG 0

LM_NAMESPACE_BEGIN

///! Combining PSSMLT and MLT via inversemap (fixed path length)
class Renderer_Invmap_MLTInvmapFixed final : public Renderer
{
public:

    LM_IMPL_CLASS(Renderer_Invmap_MLTInvmapFixed, Renderer);

public:

    int numVertices_;
    long long numMutations_;
    long long numSeedSamples_;

public:

    LM_IMPL_F(Initialize) = [this](const PropertyNode* prop) -> bool
    {
        if (!prop->ChildAs<int>("num_vertices", numVertices_)) return false;
        if (!prop->ChildAs<long long>("num_mutations", numMutations_)) return false;
        if (!prop->ChildAs<long long>("num_seed_samples", numSeedSamples_)) return false;
        return true;
    };
    
    LM_IMPL_F(Render) = [this](const Scene* scene, Random* initRng, Film* film) -> void
    {
        #if INVERSEMAP_MLTINVMAPFIXED_DEBUG
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
                ctx.b += (p->EvaluateF(0) / p->EvaluatePathPDF(scene, 0)).Luminance();
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
            
            #pragma region Thread-specific context
            struct Context
            {
                Random rng;
                Film::UniquePtr film{ nullptr, nullptr };
                std::vector<Float> currPS;
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

                    ctx.currPS = ps;
                    break;
                }
            }
            #pragma endregion

            // --------------------------------------------------------------------------------

            Parallel::For(numMutations_, [&](long long index, int threadid, bool init) -> void
            {
                auto& ctx = contexts[threadid];

                // --------------------------------------------------------------------------------

                const Float SelectPSProb = 0_f;
                if (ctx.rng.Next() < SelectPSProb)
                {
                    #pragma region Small step mutation in primary sample space
                    [&]() -> void
                    {
                        // Large step mutation
                        #if 0
                        const auto LargeStep = [this](const std::vector<Float>& currPS, Random& rng) -> std::vector <Float>
                        {
                            assert(currPS.size() == numVertices_);
                            std::vector<Float> propPS;
                            for (int i = 0; i < InversemapUtils::NumSamples(numVertices_); i++)
                            {
                                propPS.push_back(rng.Next());
                            }
                            return propPS;
                        };
                        #endif

                        // Small step mutation
                        const auto SmallStep = [this](const std::vector<Float>& ps, Random& rng) -> std::vector<Float>
                        {
                            const auto Perturb = [](Random& rng, const Float u, const Float s1, const Float s2)
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
                            };

                            std::vector<Float> propPS;
                            for (const Float u : ps)
                            {
                                propPS.push_back(Perturb(rng, u, 1_f / 1024_f, 1_f / 64_f));
                            }

                            return propPS;
                        };

                        // Function to compute path contribution
                        const auto PathContrb = [&](const Path& path) -> SPD
                        {
                            const auto F = path.EvaluateF(0);
                            assert(!F.Black());
                            SPD C;
                            if (!F.Black())
                            {
                                const auto P = path.EvaluatePathPDF(scene, 0);
                                assert(P > 0);
                                C = F / P;
                            }
                            return C;
                        };

                        // --------------------------------------------------------------------------------

                        // Mutate
                        auto propPS = SmallStep(ctx.currPS, ctx.rng);

                        // Map primary samples to paths
                        const auto currP = InversemapUtils::MapPS2Path(scene, ctx.currPS);
                        const auto propP = InversemapUtils::MapPS2Path(scene, propPS);

                        // Immediately rejects if the proposed path is invalid or the dimension changes
                        if (!propP || currP->vertices.size() != propP->vertices.size())
                        {
                            return;
                        }

                        // Evaluate contributions
                        const Float currC = PathContrb(*currP).Luminance();
                        const Float propC = PathContrb(*propP).Luminance();

                        // Acceptance ratio
                        const Float A = currC == 0 ? 1 : Math::Min(1_f, propC / currC);

                        // Accept or reject?
                        if (ctx.rng.Next() < A)
                        {
                            ctx.currPS.swap(propPS);
                        }
                    }();
                    #pragma endregion
                }
                else
                {
                    #pragma region Bidirectional mutation in path space
                    [&]() -> void
                    {
                        #pragma region Map to path space
                        auto currP = [&]() -> Path
                        {
                            const auto path = InversemapUtils::MapPS2Path(scene, ctx.currPS);
                            assert(path);
                            assert(!path->EvaluateF(0).Black());
                            return *path;
                        }();
                        #pragma endregion

                        // --------------------------------------------------------------------------------

                        #pragma region Mutate the current path
                        // Bidirectional mutation first narrows the mutation space by limiting the deleted range
                        // in the current path, so it requires some additional information other than proposed path itself.
                        struct Prop
                        {
                            Path p;
                            int kd;
                            int dL;
                        };

                        const auto prop = [&]() -> boost::optional<Prop>
                        {
                            // Implements bidirectional mutation within same path length
                            // Some simplification
                            //   - Mutation within the same path length
                
                            const int n = (int)(currP.vertices.size());

                            // Choose # of path vertices to be deleted
                            TwoTailedGeometricDist removedPathVertexNumDist(2);
                            removedPathVertexNumDist.Configure(1, 1, n);
                            const int kd = removedPathVertexNumDist.Sample(ctx.rng.Next());

                            // Choose range of deleted vertices [dL,dM]
                            const int dL = Math::Clamp((int)(ctx.rng.Next() * (n - kd + 1)), 0, n - kd);
                            const int dM = dL + kd - 1;

                            // Choose # of vertices added from each endpoint
                            const int aL = Math::Clamp((int)(ctx.rng.Next() * (kd + 1)), 0, kd);
                            const int aM = kd - aL;

                            // Sample subpaths
                            Subpath subpathL;
                            for (int s = 0; s < dL; s++)
                            {
                                subpathL.vertices.push_back(currP.vertices[s]);
                            }
                            if (subpathL.SampleSubpathFromEndpoint(scene, &ctx.rng, TransportDirection::LE, aL) != aL)
                            {
                                return boost::none;
                            }

                            Subpath subpathE;
                            for (int t = n - 1; t > dM; t--)
                            {
                                subpathE.vertices.push_back(currP.vertices[t]);
                            }
                            if (subpathE.SampleSubpathFromEndpoint(scene, &ctx.rng, TransportDirection::EL, aM) != aM)
                            {
                                return boost::none;
                            }

                            // Create proposed path
                            Prop prop;
                            if (!prop.p.ConnectSubpaths(scene, subpathL, subpathE, (int)(subpathL.vertices.size()), (int)(subpathE.vertices.size())))
                            {
                                return boost::none;
                            }

                            prop.kd = kd;
                            prop.dL = dL;
                            return prop;
                        }();
                        if (!prop)
                        {
                            return;
                        }

                        const auto Q = [&](const Path& x, const Path& y, int kd, int dL) -> SPD
                        {
                            SPD sum;
                            for (int i = 0; i <= kd; i++)
                            {
                                const auto f = y.EvaluateF(dL + i);
                                if (f.Black())
                                {
                                    return SPD();
                                }
                                const auto p = y.EvaluatePathPDF(scene, dL + i);
                                assert(p.v > 0_f);
                                const auto C = f / p;
                                sum += 1_f / C;
                            }
                            return sum;
                        };
                        #pragma endregion

                        // --------------------------------------------------------------------------------

                        #pragma region MH update
                        {
                            const auto Qxy = Q(currP, prop->p, prop->kd, prop->dL).Luminance();
                            const auto Qyx = Q(prop->p, currP, prop->kd, prop->dL).Luminance();
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
                                currP = prop->p;
                            }
                            else
                            {
                                // This is critical
                                return;
                            }
                        }
                        #pragma endregion

                        // --------------------------------------------------------------------------------


                        #pragma region Map to primary sample space
                        const auto ps = InversemapUtils::MapPath2PS(currP);
                        const auto currP2 = InversemapUtils::MapPS2Path(scene, ps);
                        if (!currP2)
                        {
                            // This sometimes happens due to numerical problem
                            #if 0
                            LM_LOG_INFO("!currP2");
                            static long long count = 0;
                            if (count == 0)
                            {
                                boost::filesystem::remove("dirs.out");
                            }
                            if (count < 10)
                            {
                                count++;
                                std::ofstream out("dirs.out", std::ios::out | std::ios::app);
                                for (const auto& v : currP.vertices)
                                {
                                    out << boost::str(boost::format("%.10f %.10f %.10f ") % v.geom.p.x % v.geom.p.y % v.geom.p.z);
                                }
                                out << std::endl;
                            }
                            #endif
                            return;
                        }
                        ctx.currPS = ps;
                        #pragma endregion
                    }();
                    #pragma endregion
                }

                // --------------------------------------------------------------------------------

                #pragma region Accumulate contribution
                {
                    auto currP = InversemapUtils::MapPS2Path(scene, ctx.currPS);
                    const auto currF = currP->EvaluateF(0);
                    if (!currF.Black())
                    {
                        const auto I = (currF / currP->EvaluatePathPDF(scene, 0)).Luminance();
                        ctx.film->Splat(currP->RasterPosition(), b / I);
                    }
                }
                //{
                //    auto currP = InversemapUtils::MapPS2Path(scene, ctx.currPS);
                //    const auto currF = currP->EvaluateF(0);
                //    if (!currF.Black())
                //    {
                //        const auto I = (currF / currP->EvaluatePathPDF(scene, 0)).Luminance();
                //        ctx.film->Splat(currP->RasterPosition(), b / I);
                //    }
                //}
                #pragma endregion
            });

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

LM_COMPONENT_REGISTER_IMPL(Renderer_Invmap_MLTInvmapFixed, "renderer::invmap_mltinvmapfixed");

LM_NAMESPACE_END
