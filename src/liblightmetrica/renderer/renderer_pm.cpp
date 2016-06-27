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
#include <lightmetrica/scheduler.h>
#include <lightmetrica/scene.h>
#include <lightmetrica/primitive.h>
#include <lightmetrica/surfacegeometry.h>
#include <lightmetrica/ray.h>
#include <lightmetrica/intersection.h>
#include <lightmetrica/random.h>
#include <lightmetrica/light.h>
#include <lightmetrica/sensor.h>
#include <lightmetrica/film.h>
#include <lightmetrica/detail/photonmap.h>
#include <lightmetrica/detail/photonmaputils.h>
#include <lightmetrica/detail/parallel.h>

LM_NAMESPACE_BEGIN

/*!
    \brief Photon mapping renderer.
    Implements photon mapping.
    References:
      - H. W. Jensen, Global illumination using photon maps,
        Procs. of the Eurographics Workshop on Rendering Techniques 96, pp.21-30, 1996.
      - H. W. Jensen, Realistic image synthesis using photon mapping,
        AK Peters, 2001
*/
class Renderer_PM final : public Renderer
{
public:

    LM_IMPL_CLASS(Renderer_PM, Renderer);

public:

    LM_IMPL_F(Initialize) = [this](const PropertyNode* prop) -> bool
    {
        sched_->Load(prop);
        maxNumVertices_ = prop->ChildAs<int>("max_num_vertices", -1);
        numPhotonTraceSamples_ = prop->ChildAs<long long>("num_photon_trace_samples", 100000L);
        finalgather_ = prop->ChildAs<int>("finalgather", 1);
        radius_ = prop->ChildAs<Float>("radius", 0.01_f);
        pm_ = ComponentFactory::Create<PhotonMap>("photonmap::" + prop->ChildAs<std::string>("photonmap", "kdtree"));
        return true;
    };

    LM_IMPL_F(Render) = [this](const Scene* scene, Random* initRng, Film* film_) -> void
    {
        #pragma region Trace photons
        std::vector<Photon> photons;
        {
            LM_LOG_INFO("Tracing photons");
            LM_LOG_INDENTER();
                
            struct Context
            {
                Random rng;
                std::vector<Photon> photons;
            };
            std::vector<Context> contexts(Parallel::GetNumThreads());
            for (auto& ctx : contexts)
            {
                ctx.rng.SetSeed(initRng->NextUInt());
            }

            Parallel::For(numPhotonTraceSamples_, [&](long long index, int threadid, bool init)
            {
                auto& ctx = contexts[threadid];
                PhotonMapUtils::TraceSubpath(scene, &ctx.rng, maxNumVertices_, TransportDirection::LE, [&](int numVertices, const Vec2& /*rasterPos*/, const PhotonMapUtils::PathVertex& pv, const PhotonMapUtils::PathVertex& v, SPD& throughput) -> bool
                {
                    // Record photon
                    if ((v.type & SurfaceInteractionType::D) > 0 || (v.type & SurfaceInteractionType::G) > 0)
                    {
                        Photon photon;
                        photon.p = v.geom.p;
                        photon.throughput = throughput;
                        photon.wi = Math::Normalize(pv.geom.p - v.geom.p);
                        photon.numVertices = numVertices;
                        ctx.photons.push_back(photon);
                    }

                    // Path termination
                    const Float rrProb = 0.5_f;
                    if (ctx.rng.Next() > rrProb)
                    {
                        return false;
                    }
                    else
                    {
                        throughput /= rrProb;
                    }

                    return true;
                });
            });

            for (auto& ctx : contexts)
            {
                photons.insert(photons.end(), ctx.photons.begin(), ctx.photons.end());
            }
        }
        #pragma endregion

        // --------------------------------------------------------------------------------

        #pragma region Build photon map
        {
            LM_LOG_INFO("Building photon map");
            LM_LOG_INDENTER();
            pm_->Build(std::move(photons));
        }
        #pragma endregion

        // --------------------------------------------------------------------------------

        #pragma region Trace eye rays
        sched_->Process(scene, film_, initRng, [&](Film* film, Random* rng)
        {
            bool gatherNext = !finalgather_;
            PhotonMapUtils::TraceSubpath(scene, rng, maxNumVertices_, TransportDirection::EL, [&](int numVertices, const Vec2& rasterPos, const PhotonMapUtils::PathVertex& pv, const PhotonMapUtils::PathVertex& v, SPD& throughput) -> bool
            {
                // Skip initial vertex
                if (numVertices == 1)
                {
                    return true;
                }

                // Handle hit with light source
                if ((v.primitive->Type() & SurfaceInteractionType::L) > 0)
                {
                    // Accumulate to film
                    const auto C =
                        throughput
                        * v.primitive->EvaluateDirection(v.geom, SurfaceInteractionType::L, Vec3(), Math::Normalize(pv.geom.p - v.geom.p), TransportDirection::EL, false)
                        * v.primitive->EvaluatePosition(v.geom, false);
                    film->Splat(rasterPos, C);
                }

                // Photon density estimation
                if ((v.type & SurfaceInteractionType::D) > 0 || (v.type & SurfaceInteractionType::G) > 0)
                {
                    if (gatherNext)
                    {
                        // Density estimation
                        const auto Kernel = [](const Vec3& p, const Photon& photon, Float radius)
                        {
                            auto s = 1_f - Math::Length2(photon.p - p) / radius / radius;
                            return 3_f * Math::InvPi() * s * s;
                        };
                        pm_->CollectPhotons(v.geom.p, radius_, [&](const Photon& photon) -> void
                        {
                            if (numVertices + photon.numVertices - 1 > maxNumVertices_)
                            {
                                return;
                            }
                            auto k = Kernel(v.geom.p, photon, radius_);
                            auto p = k / (radius_ * radius_ * numPhotonTraceSamples_);
                            const auto f = v.primitive->EvaluateDirection(v.geom, SurfaceInteractionType::BSDF, Math::Normalize(pv.geom.p - v.geom.p), photon.wi, TransportDirection::EL, true);
                            const auto C = throughput * p * f * photon.throughput;
                            film->Splat(rasterPos, C);
                        });

                        #pragma endregion

                        // TODO: support multi component materials
                        return false;
                    }

                    gatherNext = true;
                    return true;
                }

                return true;
            });
        });
        #pragma endregion
    };

private:

    int maxNumVertices_;
    long long numPhotonTraceSamples_;
    int finalgather_;
    Float radius_;
    Scheduler::UniquePtr sched_ = ComponentFactory::Create<Scheduler>();
    PhotonMap::UniquePtr pm_{ nullptr, nullptr };

};

LM_COMPONENT_REGISTER_IMPL(Renderer_PM, "renderer::pm");

LM_NAMESPACE_END
