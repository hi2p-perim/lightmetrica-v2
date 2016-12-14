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
#include "debugio.h"
#include <mutex>
#include <cereal/archives/json.hpp>
#include <cereal/types/vector.hpp>

#define INVERSEMAP_MLTFIXED_DEBUG_OUTPUT_TRIANGLES 0
#define INVERSEMAP_MLTFIXED_DEBUG_OUTPUT_SAMPLED_PATHS 0
#define INVERSEMAP_MLTFIXED_DEBUG_LONGEST_REJECTION 0
#define INVERSEMAP_MLTINVMAPFIXED_DEBUG_LENS_PERTURB_SUBSPACE_CONSISTENCY 0

LM_NAMESPACE_BEGIN

///! Metropolis light transport (fixed path length)
class Renderer_Invmap_MLTFixed final : public Renderer
{
public:

    LM_IMPL_CLASS(Renderer_Invmap_MLTFixed, Renderer);

public:

    int numVertices_;
    long long numMutations_;
    long long numSeedSamples_;
    MLTMutationStrategy mut_;
    std::vector<Float> initStrategyWeights_{ 1_f, 1_f, 1_f, 1_f, 1_f, 1_f, 1_f, 1_f };
    #if INVERSEMAP_OMIT_NORMALIZATION
    Float normalization_;
    #endif
    std::string pathType_;

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
            initStrategyWeights_[(int)(MLTStrategy::Bidir)]           = child->ChildAs<Float>("bidir", 1_f);
            initStrategyWeights_[(int)(MLTStrategy::Lens)]            = child->ChildAs<Float>("lens", 1_f);
            initStrategyWeights_[(int)(MLTStrategy::Caustic)]         = child->ChildAs<Float>("caustic", 1_f);
            initStrategyWeights_[(int)(MLTStrategy::Multichain)]      = child->ChildAs<Float>("multichain", 1_f);
            initStrategyWeights_[(int)(MLTStrategy::ManifoldLens)]    = child->ChildAs<Float>("manifoldlens", 1_f);
            initStrategyWeights_[(int)(MLTStrategy::ManifoldCaustic)] = child->ChildAs<Float>("manifoldcaustic", 1_f);
            initStrategyWeights_[(int)(MLTStrategy::Manifold)]        = child->ChildAs<Float>("manifold", 1_f);
            initStrategyWeights_[(int)(MLTStrategy::Identity)]        = child->ChildAs<Float>("identity", 0_f);
        }
        #if INVERSEMAP_OMIT_NORMALIZATION
        normalization_ = prop->ChildAs<Float>("normalization", 1_f);
        #endif
        pathType_ = prop->ChildAs<std::string>("path_type", "");
        return true;
    };

    LM_IMPL_F(Render) = [this](const Scene* scene, Random* initRng, const std::string& outputPath) -> void
    {
        #if INVERSEMAP_MLT_DEBUG_IO
        DebugIO::Run();
        #endif

        // --------------------------------------------------------------------------------

        auto* film = static_cast<const Sensor*>(scene->GetSensor()->emitter)->GetFilm();

        // --------------------------------------------------------------------------------

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

        #if INVERSEMAP_MLT_DEBUG_IO
        LM_LOG_DEBUG("triangle_vertices");
        {
            DebugIO::Wait();

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

            DebugIO::Output("triangle_vertices", ss.str());
            DebugIO::Wait();
        }
        #endif

        // --------------------------------------------------------------------------------

        #pragma region Compute normalization factor
        #if INVERSEMAP_OMIT_NORMALIZATION
        const auto b = normalization_;
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
#if 1
                while (true)
                {
                    // Generate initial path with bidirectional path tracing
                    const auto path = [&]() -> boost::optional<Path>
                    {
                        Subpath subpathE;
                        Subpath subpathL;
                        subpathE.SampleSubpathFromEndpoint(scene, &ctx.rng, TransportDirection::EL, numVertices_);
                        subpathL.SampleSubpathFromEndpoint(scene, &ctx.rng, TransportDirection::LE, numVertices_);

                        const int nE = (int)(subpathE.vertices.size());
                        for (int t = 1; t <= nE; t++)
                        {
                            const int nL = (int)(subpathL.vertices.size());
                            const int minS = Math::Max(0, Math::Max(2 - t, numVertices_ - t));
                            const int maxS = Math::Min(nL, numVertices_ - t);
                            for (int s = minS; s <= maxS; s++)
                            {
                                if (s + t != numVertices_) { continue; }
                                Path fullpath;
                                if (!fullpath.ConnectSubpaths(scene, subpathL, subpathE, s, t)) { continue; }
                                if (!fullpath.IsPathType(pathType_)) { continue; }
                                const auto Cstar = fullpath.EvaluateUnweightContribution(scene, s);
                                if (Cstar.Black())
                                {
                                    continue;
                                }
                                return fullpath;
                            }
                        }

                        return boost::none;
                    }();
                    if (!path)
                    {
                        continue;
                    }

                    // Convert the path to the primary sample with cdf(path).
                    const auto ps = InversemapUtils::MapPath2PS(*path, initRng);

                    // Sanity check
                    const auto path2 = InversemapUtils::MapPS2Path(scene, ps);
                    if (!path2)
                    {
                        continue;
                    }
                    const auto f1 = path->EvaluateF(0).Luminance();
                    const auto f2 = path2->EvaluateF(0).Luminance();
                    if (Math::Abs(f1 - f2) > Math::Eps())
                    {
                        continue;
                    }

                    ctx.currP = *path;
                    break;
                }
#else
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
#endif
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
                    const auto strategy = [&]() -> MLTStrategy
                    {
                        const auto StrategyDist = [&]() -> Distribution1D
                        {
                            Distribution1D dist;
                            for (size_t i = 0; i < initStrategyWeights_.size(); i++)
                            {
                                const auto w = initStrategyWeights_[i];
                                if (MLTMutationStrategy::CheckMutatable((MLTStrategy)(i), ctx.currP))
                                {
                                    dist.Add(w);
                                }
                                else
                                {
                                    dist.Add(0_f);
                                }
                            }
                            dist.Normalize();
                            return dist;
                        }();
                        return (MLTStrategy)(StrategyDist.Sample(ctx.rng.Next()));
                    }();
                    #pragma endregion

                    // --------------------------------------------------------------------------------

                    #pragma region Mutate the current path
                    const auto prop = MLTMutationStrategy::Mutate(strategy, scene, ctx.rng, ctx.currP);
                    if (!prop)
                    {
                        return false;
                    }
                    #pragma endregion

                    // --------------------------------------------------------------------------------

                    #pragma region MH update
                    {
                        const auto Qxy = MLTMutationStrategy::Q(strategy, scene, ctx.currP, prop->p, prop->subspace);
                        const auto Qyx = MLTMutationStrategy::Q(strategy, scene, prop->p, ctx.currP, prop->subspace);
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

                #if LM_DEBUG_MODE
                static long long lastAcceptIndex = 0;
                if (accept)
                {
                    lastAcceptIndex = index;
                }
                #endif

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
                    if (!currF.Black() && ctx.currP.IsPathType(pathType_))
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

        // --------------------------------------------------------------------------------

        #if INVERSEMAP_DEBUG_MLT_MANIFOLDWALK_STAT
        MLTMutationStrategy::PrintStat();
        #endif

        // --------------------------------------------------------------------------------

        #pragma region Save image
        {
            LM_LOG_INFO("Saving image");
            LM_LOG_INDENTER();
            film->Save(outputPath);
        }
        #pragma endregion

        // --------------------------------------------------------------------------------

        #if INVERSEMAP_MLT_DEBUG_IO
        DebugIO::Stop();
        #endif
    };

};

LM_COMPONENT_REGISTER_IMPL(Renderer_Invmap_MLTFixed, "renderer::invmap_mltfixed");

LM_NAMESPACE_END
