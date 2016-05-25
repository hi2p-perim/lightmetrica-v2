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

#include <pch.h>
#include <lightmetrica/renderer.h>
#include <lightmetrica/property.h>
#include <lightmetrica/random.h>
#include <lightmetrica/scene.h>
#include <lightmetrica/film.h>
#include <lightmetrica/scheduler.h>
#include <lightmetrica/renderutils.h>
#include <lightmetrica/primitive.h>
#include <lightmetrica/detail/parallel.h>
#include <lightmetrica/detail/photonmap.h>
#include <lightmetrica/detail/photonmaputils.h>
#include <lightmetrica/detail/vcmutils.h>

#define LM_BDPM_DEBUG 1

LM_NAMESPACE_BEGIN

///! Implements progressive BDPM as an intermediate implementation of VCM
class Renderer_VCM_BDPM_Progressive final : public Renderer
{
public:

    LM_IMPL_CLASS(Renderer_VCM_BDPM_Progressive, Renderer);

private:

    int maxNumVertices_;
    int minNumVertices_;
    long long numPhotonPass_;
    long long numPhotonTraceSamples_;
    long long numEyeTraceSamples_;
    Float initialRadius_;
    Float alpha_;
    Scheduler::UniquePtr sched_ = ComponentFactory::Create<Scheduler>();

public:

    LM_IMPL_F(Initialize) = [this](const PropertyNode* prop) -> bool
    {
        sched_->Load(prop);
        maxNumVertices_        = prop->Child("max_num_vertices")->As<int>();
        minNumVertices_        = prop->Child("min_num_vertices")->As<int>();
        numPhotonPass_         = prop->ChildAs<long long>("num_photon_pass", 1000L);
        numPhotonTraceSamples_ = prop->ChildAs<long long>("num_photon_trace_samples", 100L);
        numEyeTraceSamples_    = prop->ChildAs<long long>("num_eye_trace_samples", 100L);
        initialRadius_         = prop->ChildAs<Float>("initial_radius", 0.1_f);
        alpha_                 = prop->ChildAs<Float>("alpha", 0.7_f);
        return true;
    };

    LM_IMPL_F(Render) = [this](const Scene* scene, Random* initRng, Film* film) -> void
    {
        Float mergeRadius = 0_f;
        for (long long pass = 0; pass < numPhotonPass_; pass++)
        {
            LM_LOG_INFO("Pass " + std::to_string(pass));
            LM_LOG_INDENTER();

            // --------------------------------------------------------------------------------

            #pragma region Update merge radius
            mergeRadius = pass == 0 ? initialRadius_ : std::sqrt((alpha_ + pass) / (1_f + pass)) * mergeRadius;
            #pragma endregion

            // --------------------------------------------------------------------------------

            #pragma region Sample light subpaths
            std::vector<Subpath> subpathLs;
            {
                LM_LOG_INFO("Sampling light subpaths");
                LM_LOG_INDENTER();

                struct Context
                {
                    Random rng;
                    std::vector<Subpath> subpathLs;
                };
                std::vector<Context> contexts(Parallel::GetNumThreads());
                for (auto& ctx : contexts) { ctx.rng.SetSeed(initRng->NextUInt()); }

                Parallel::For(numPhotonTraceSamples_, [&](long long index, int threadid, bool init)
                {
                    auto& ctx = contexts[threadid];
                    ctx.subpathLs.emplace_back();
                    ctx.subpathLs.back().SampleSubpath(scene, &ctx.rng, TransportDirection::LE, maxNumVertices_);
                });

                for (auto& ctx : contexts)
                {
                    subpathLs.insert(subpathLs.end(), ctx.subpathLs.begin(), ctx.subpathLs.end());
                }
            }
            #pragma endregion

            // --------------------------------------------------------------------------------

            #pragma region Construct range query structure for vertices in light subpaths
            LM_LOG_INFO("Constructing range query structure");
            VCMKdTree pm(subpathLs);
            #pragma endregion

            // --------------------------------------------------------------------------------

            #pragma region Estimate contribution
            {
                LM_LOG_INFO("Estimating contribution");
                LM_LOG_INDENTER();

                struct Context
                {
                    Random rng;
                    Film::UniquePtr film{ nullptr, nullptr };
                };
                std::vector<Context> contexts(Parallel::GetNumThreads());
                for (auto& ctx : contexts)
                {
                    ctx.rng.SetSeed(initRng->NextUInt());
                    ctx.film = ComponentFactory::Clone<Film>(film);
                    ctx.film->Clear();
                }

                Parallel::For(numEyeTraceSamples_, [&](long long index, int threadid, bool init)
                {
                    auto& ctx = contexts[threadid];

                    // --------------------------------------------------------------------------------

                    // Sample eye subpath
                    Subpath subpathE;
                    subpathE.SampleSubpath(scene, &ctx.rng, TransportDirection::EL, maxNumVertices_);

                    // --------------------------------------------------------------------------------

                    // Combine subpaths
                    const int nE = (int)(subpathE.vertices.size());
                    for (int t = 1; t <= nE; t++)
                    {
                        const auto& vE = subpathE.vertices[t - 1];
                        if (vE.primitive->surface->IsDeltaPosition(vE.type))
                        {
                            continue;
                        }
                        pm.RangeQuery(vE.geom.p, mergeRadius, [&](int si, int vi) -> void
                        {
                            const int s = vi + 1;
                            const int n = s + t - 1;
                            if (n < minNumVertices_ || maxNumVertices_ < n) { return; }

                            // Merge vertices and create a full path
                            Path fullpath;
                            if (!fullpath.MergeSubpaths(subpathLs[si], subpathE, s - 1, t)) { return; }

                            // Evaluate contribution
                            const auto f = fullpath.EvaluateF(s - 1, true);
                            if (f.Black()) { return; }

                            // Evaluate path PDF
                            const auto p = fullpath.EvaluatePathPDF(scene, s - 1, true, mergeRadius);

                            // Evaluate MIS weight
                            const auto w = fullpath.EvaluateMISWeight_BDPM(scene, s - 1, mergeRadius, numPhotonTraceSamples_);

                            // Accumulate contribution
                            const auto C = f * w / p;
                            ctx.film->Splat(fullpath.RasterPosition(), C * (Float)(film->Width() * film->Height()) / (Float)numEyeTraceSamples_);
                        });
                    }
                });

                film->Rescale((Float)(pass) / (1_f + pass));
                for (auto& ctx : contexts)
                {
                    ctx.film->Rescale(1_f / (1_f + pass));
                    film->Accumulate(ctx.film.get());
                }
            }
            #pragma endregion

            // --------------------------------------------------------------------------------

            #if LM_BDPM_DEBUG
            {
                boost::format f("bdpm_%05d");
                f.exceptions(boost::io::all_error_bits ^ (boost::io::too_many_args_bit | boost::io::too_few_args_bit));
                film->Save(boost::str(f % pass));
            }
            #endif
        }
    };

};

LM_COMPONENT_REGISTER_IMPL(Renderer_VCM_BDPM_Progressive, "renderer::vcmbdpmprog");

LM_NAMESPACE_END
