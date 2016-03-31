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

LM_NAMESPACE_BEGIN

#pragma region Photon map

struct Photon : public SIMDAlignedType
{
    Vec3 p;          // Surface point
    SPD throughput;  // Current throughput
    Vec3 wi;         // Incident ray direction
    int numVertices;
};

struct PhotonMap : public Component
{
    LM_INTERFACE_CLASS(PhotonMap, Component, 0);
    virtual auto Build(const std::vector<Photon>& photons) -> void = 0;
    virtual auto CollectPhotons(const Vec3& p, Float& maxDist2, const std::function<void(const Photon&, Float&)>& collectFunc) const -> void = 0;
};

class PhotonMap_Naive : public PhotonMap
{
public:

    LM_IMPL_CLASS(PhotonMap_Naive, PhotonMap);
    
public:

    virtual auto Build(const std::vector<Photon>& photons) -> void
    {
        photons_ = photons;
    }

    virtual auto CollectPhotons(const Vec3& p, Float& maxDist2, const std::function<void(const Photon&, Float&)>& collectFunc) const -> void
    {
        for (const auto& photon : photons_)
        {
            const auto dist2 = Math::Length2(photon.p - p);
            if (dist2 < maxDist2)
            {
                collectFunc(photon, maxDist2);
            }
        }
    }

private:

    std::vector<Photon> photons_;

};

struct PhotonKdTreeNode
{
    bool isleaf;
    Bound bound;
    
    union
    {
        struct
        {
            int begin;
            int end;
        } leaf;

        struct
        {
            int child1;
            int child2;
        } internal;
    };
};

class PhotonMap_KdTree : public PhotonMap
{
public:

    LM_IMPL_CLASS(PhotonMap_KdTree, PhotonMap);

public:

    virtual auto Build(const std::vector<Photon>& photons) -> void
    {
        // Build function
        const std::function<int(int, int)> Build_ = [&](int begin, int end) -> int
        {
            int idx = (int)(nodes_.size());
            nodes_.emplace_back(new PhotonKdTreeNode);
            auto* node = nodes_[idx].get();

            // Current bound
            node->bound = Bound();
            for (int i = begin; i < end; i++)
            {
                node->bound = Math::Union(node->bound, photons_[indices_[i]].p);
            }

            // Create leaf node
            const int LeafNumNodes = 10;
            if (end - begin < LeafNumNodes)
            {
                node->isleaf = true;
                node->leaf.begin = begin;
                node->leaf.end = end;
                return idx;
            }

            // Select longest axis as split axis
            const int axis = node->bound.LongestAxis();

            // Select split position
            const Float split = node->bound.Centroid()[axis];

            // Partition into two sets according to split position
            const auto it = std::partition(indices_.begin() + begin, indices_.begin() + end, [&](int i) -> bool
            {
                return photons_[i].p[axis] < split;
            });

            // Create intermediate node
            const int mid = (int)(std::distance(indices_.begin(), it));
            node->isleaf = false;
            node->internal.child1 = Build_(begin, mid);
            node->internal.child2 = Build_(mid, end);

            return idx;
        };

        photons_ = photons;
        nodes_.clear();
        indices_.assign(photons.size(), 0);
        std::iota(indices_.begin(), indices_.end(), 0);
        Build_(0, (int)(photons.size()));
    }

    virtual auto CollectPhotons(const Vec3& p, Float& maxDist2, const std::function<void(const Photon&, Float&)>& collectFunc) const -> void
    {
        const std::function<void(int)> Collect = [&](int idx) -> void
        {
            const auto* node = nodes_.at(idx).get();
            
            if (node->isleaf)
            {
                for (int i = node->leaf.begin; i < node->leaf.end; i++)
                {
                    const auto& photon = photons_[indices_[i]];
                    const auto dist2 = Math::Length2(photon.p - p);
                    if (dist2 < maxDist2)
                    {
                        collectFunc(photon, maxDist2);
                    }
                }
                return;
            }

            const int axis = node->bound.LongestAxis();
            const Float split = node->bound.Centroid()[axis];
            const auto dist2 = (p[axis] - split) * (p[axis] - split);
            if (p[axis] < split)
            {
                Collect(node->internal.child1);
                if (dist2 < maxDist2)
                {
                    Collect(node->internal.child2);
                }
            }
            else
            {
                Collect(node->internal.child2);
                if (dist2 < maxDist2)
                {
                    Collect(node->internal.child1);
                }
            }
        };

        Collect(0);
    }

private:

    std::vector<std::unique_ptr<PhotonKdTreeNode>> nodes_;
    std::vector<int> indices_;
    std::vector<Photon> photons_;

};

LM_COMPONENT_REGISTER_IMPL(PhotonMap_Naive, "photonmap::naive");
LM_COMPONENT_REGISTER_IMPL(PhotonMap_KdTree, "photonmap::kdtree");

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region PM renderer

/*!
    Photon mapping renderer.
    Implements photon mapping. Unoptimized version.
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
        maxNumVertices_ = prop->Child("max_num_vertices")->As<int>();
        maxPhotons_ = prop->ChildAs<int>("max_photons", 1000);
        finalgather_ = prop->ChildAs<int>("finalgather", 1);
        pm_ = ComponentFactory::Create<PhotonMap>("photonmap::" + prop->ChildAs<std::string>("photonmap", "kdtree"));
        return true;
    };

    LM_IMPL_F(Render) = [this](const Scene* scene, Film* film) -> void
    {
        Random initRng;
        #if LM_DEBUG_MODE
        initRng.SetSeed(1008556906);
        #else
        initRng.SetSeed(static_cast<unsigned int>(std::time(nullptr)));
        #endif

        // --------------------------------------------------------------------------------

        #pragma region Trace photons

        std::vector<Photon> photons;
        long long tracedLightPaths = 0;
        {
            LM_LOG_INFO("Tracing photons");
            LM_LOG_INDENTER();

            std::unique_ptr<Random> rng(new Random);
            rng->SetSeed(initRng.NextUInt());

            for (tracedLightPaths = 0; (int)(photons.size()) < maxPhotons_; tracedLightPaths++)
            {
                #pragma region Sample a light

                const auto* L = scene->SampleEmitter(SurfaceInteractionType::L, rng->Next());
                const auto pdfL = scene->EvaluateEmitterPDF(L);
                assert(pdfL > 0_f);

                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Sample a position on the light and initial ray direction

                SurfaceGeometry geomL;
                Vec3 initWo;
                L->light->SamplePositionAndDirection(rng->Next2D(), rng->Next2D(), geomL, initWo);
                const auto pdfPL = L->light->EvaluatePositionGivenDirectionPDF(geomL, initWo, false);
                assert(pdfPL > 0_f);

                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Temporary variables

                auto throughput = L->light->EvaluatePosition(geomL, false) / pdfPL / pdfL;
                const auto* primitive = L;
                int type = SurfaceInteractionType::L;
                auto geom = geomL;
                Vec3 wi;
                int numVertices = 1;

                #pragma endregion

                // --------------------------------------------------------------------------------

                while (true)
                {
                    if (maxNumVertices_ != -1 && numVertices >= maxNumVertices_)
                    {
                        break;
                    }

                    // --------------------------------------------------------------------------------

                    #pragma region Sample next direction

                    Vec3 wo;
                    if (type == SurfaceInteractionType::L)
                    {
                        wo = initWo;
                    }
                    else
                    {
                        primitive->surface->SampleDirection(rng->Next2D(), rng->Next(), type, geom, wi, wo);
                    }
                    const auto pdfD = primitive->surface->EvaluateDirectionPDF(geom, type, wi, wo, false);

                    #pragma endregion

                    // --------------------------------------------------------------------------------

                    #pragma region Evaluate direction

                    const auto fs = primitive->surface->EvaluateDirection(geom, type, wi, wo, TransportDirection::LE, false);
                    if (fs.Black())
                    {
                        break;
                    }

                    #pragma endregion

                    // --------------------------------------------------------------------------------

                    #pragma region Update throughput

                    assert(pdfD > 0_f);
                    throughput *= fs / pdfD;

                    #pragma endregion

                    // --------------------------------------------------------------------------------

                    #pragma region Intersection

                    // Setup next ray
                    Ray ray = { geom.p, wo };

                    // Intersection query
                    Intersection isect;
                    if (!scene->Intersect(ray, isect))
                    {
                        break;
                    }

                    if (isect.geom.infinite)
                    {
                        break;
                    }

                    #pragma endregion

                    // --------------------------------------------------------------------------------

                    #pragma region Record photon

                    if ((isect.primitive->surface->Type() & SurfaceInteractionType::D) > 0 || (isect.primitive->surface->Type() & SurfaceInteractionType::G) > 0)
                    {
                        Photon photon;
                        photon.p = isect.geom.p;
                        photon.throughput = throughput;
                        photon.wi = -ray.d;
                        photon.numVertices = numVertices + 1;
                        photons.push_back(photon);
                        if ((int)(photons.size()) == maxPhotons_)
                        {
                            break;
                        }
                    }

                    #pragma endregion

                    // --------------------------------------------------------------------------------

                    #pragma region Path termination

                    const Float rrProb = 0.5_f;
                    if (rng->Next() > rrProb)
                    {
                        break;
                    }
                    else
                    {
                        throughput /= rrProb;
                    }

                    #pragma endregion

                    // --------------------------------------------------------------------------------

                    #pragma region Update information

                    geom = isect.geom;
                    primitive = isect.primitive;
                    type = isect.primitive->surface->Type() & ~SurfaceInteractionType::Emitter;
                    wi = -ray.d;
                    numVertices++;

                    #pragma endregion
                }

                // --------------------------------------------------------------------------------

                const double progress = (double)(photons.size()) / maxPhotons_ * 100.0;
                LM_LOG_INPLACE(boost::str(boost::format("Progress: %.1f%%") % progress));
            }

            LM_LOG_INFO("Progress: 100.0%");
        }

        #pragma endregion

        // --------------------------------------------------------------------------------

        #pragma region Build photon map

        {
            LM_LOG_INFO("Building photon map");
            LM_LOG_INDENTER();
            pm_->Build(photons);
        }

        #pragma endregion

        // --------------------------------------------------------------------------------

        #pragma region Trace eye rays

        sched_->Process(scene, film, &initRng, [&tracedLightPaths, this](const Scene* scene, Film* film, Random* rng)
        {
            #pragma region Sample a sensor

            const auto* E = scene->SampleEmitter(SurfaceInteractionType::E, rng->Next());
            const auto pdfE = scene->EvaluateEmitterPDF(E);
            assert(pdfE.v > 0);

            #pragma endregion

            // --------------------------------------------------------------------------------

            #pragma region Sample a position on the sensor and initial ray direction

            SurfaceGeometry geomE;
            Vec3 initWo;
            E->sensor->SamplePositionAndDirection(rng->Next2D(), rng->Next2D(), geomE, initWo);
            const auto pdfPE = E->sensor->EvaluatePositionGivenDirectionPDF(geomE, initWo, false);
            assert(pdfPE.v > 0);

            #pragma endregion

            // --------------------------------------------------------------------------------

            #pragma region Calculate raster position for initial vertex

            Vec2 rasterPos;
            if (!E->sensor->RasterPosition(initWo, geomE, rasterPos))
            {
                // This can happen due to numerical errors
                return;
            }

            #pragma endregion

            // --------------------------------------------------------------------------------

            #pragma region Temporary variables

            auto throughput = E->sensor->EvaluatePosition(geomE, false) / pdfPE / pdfE;
            const auto* primitive = E;
            int type = SurfaceInteractionType::E;
            auto geom = geomE;
            Vec3 wi;
            int numVertices = 1;
            bool finalgather = !finalgather_;

            #pragma endregion

            // --------------------------------------------------------------------------------

            while (true)
            {
                if (maxNumVertices_ != -1 && numVertices >= maxNumVertices_)
                {
                    break;
                }
                
                // --------------------------------------------------------------------------------

                #pragma region Sample direction

                Vec3 wo;
                if (type == SurfaceInteractionType::E)
                {
                    wo = initWo;
                }
                else
                {
                    primitive->surface->SampleDirection(rng->Next2D(), rng->Next(), type, geom, wi, wo);
                }
                const auto pdfD = primitive->surface->EvaluateDirectionPDF(geom, type, wi, wo, false);

                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Evaluate direction

                const auto fs = primitive->surface->EvaluateDirection(geom, type, wi, wo, TransportDirection::EL, false);
                if (fs.Black())
                {
                    break;
                }

                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Update throughput

                assert(pdfD > 0_f);
                throughput *= fs / pdfD;

                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Intersection

                // Setup next ray
                Ray ray = { geom.p, wo };

                // Intersection query
                Intersection isect;
                if (!scene->Intersect(ray, isect))
                {
                    break;
                }

                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Handle hit with light source

                if ((isect.primitive->surface->Type() & SurfaceInteractionType::L) > 0)
                {
                    // Accumulate to film
                    const auto C =
                          throughput
                        * isect.primitive->emitter->EvaluateDirection(isect.geom, SurfaceInteractionType::L, Vec3(), -ray.d, TransportDirection::EL, false)
                        * isect.primitive->emitter->EvaluatePosition(isect.geom, false);
                    film->Splat(rasterPos, C);
                }

                if (isect.geom.infinite)
                {
                    break;
                }

                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Photon density estimation

                if ((isect.primitive->surface->Type() & SurfaceInteractionType::D) > 0 || (isect.primitive->surface->Type() & SurfaceInteractionType::G) > 0)
                {
                    if (finalgather)
                    {
                        #pragma region Collect photons

                        struct PhotonInfo
                        {
                            const Photon* photon;
                            Float dist2;
                        };

                        const size_t numNNQueryPhotons = 20;
                        Float maxDist2 = 0.1_f * 0.1_f;
                        std::vector<PhotonInfo> photonInfo;
                        pm_->CollectPhotons(isect.geom.p, maxDist2, [&](const Photon& photon, Float& maxDist2)
                        {
                            auto dist2 = Math::Length2(photon.p - isect.geom.p);
                            const auto comp = [](const PhotonInfo& p1, const PhotonInfo& p2)
                            {
                                return p1.dist2 < p2.dist2;
                            };

                            if (photonInfo.size() < numNNQueryPhotons)
                            {
                                photonInfo.push_back(PhotonInfo{ &photon, dist2 });
                                if (photonInfo.size() == numNNQueryPhotons)
                                {
                                    // Create heap
                                    std::make_heap(photonInfo.begin(), photonInfo.end(), comp);
                                    maxDist2 = photonInfo.front().dist2;
                                }
                            }
                            else
                            {
                                // Update heap
                                std::pop_heap(photonInfo.begin(), photonInfo.end(), comp);
                                photonInfo.back() = PhotonInfo{ &photon, dist2 };
                                std::push_heap(photonInfo.begin(), photonInfo.end(), comp);
                                maxDist2 = photonInfo.front().dist2;
                            }
                        });

                        #pragma endregion

                        // --------------------------------------------------------------------------------

                        #pragma region Density estimation

                        const auto Kernel = [](const Vec3& p, const Photon& photon, Float maxDist2)
                        {
                            auto s = 1_f - Math::Length2(photon.p - p) / maxDist2;
                            return 3_f * Math::InvPi() * s * s;
                        };
                        for (const auto& info : photonInfo)
                        {
                            if (numVertices + info.photon->numVertices > maxNumVertices_)
                            {
                                continue;
                            }

                            // Evaluate photon density estimation kernel
                            // Do not to forget to divide by #tracedLightPaths
                            auto k = Kernel(isect.geom.p, *info.photon, maxDist2);
                            auto p = k / (maxDist2 * tracedLightPaths);
                            const auto f = isect.primitive->surface->EvaluateDirection(isect.geom, SurfaceInteractionType::BSDF, -ray.d, info.photon->wi, TransportDirection::EL, true);
                            const auto C = throughput * p * f * info.photon->throughput;
                            film->Splat(rasterPos, C);
                        }

                        #pragma endregion

                        // --------------------------------------------------------------------------------

                        // Stop the eye subpath if intersected vertex is not specular
                        if ((isect.primitive->surface->Type() & SurfaceInteractionType::S) == 0)
                        {
                            break;
                        }
                    }

                    finalgather = true;
                }

                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Path termination

                if (isect.geom.infinite)
                {
                    break;
                }

                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Update information

                geom = isect.geom;
                primitive = isect.primitive;
                type = isect.primitive->surface->Type() & ~SurfaceInteractionType::Emitter;
                wi = -ray.d;
                numVertices++;

                #pragma endregion
            }
        });

        #pragma endregion
    };

private:

    int maxNumVertices_;
    int maxPhotons_;
    int finalgather_;
    Scheduler::UniquePtr sched_ = ComponentFactory::Create<Scheduler>();
    PhotonMap::UniquePtr pm_{ nullptr, nullptr };

};

#pragma endregion

LM_COMPONENT_REGISTER_IMPL(Renderer_PM, "renderer::pm");

LM_NAMESPACE_END
