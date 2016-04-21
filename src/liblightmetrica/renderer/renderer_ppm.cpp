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
#include <lightmetrica/bsdf.h>
#include <lightmetrica/ray.h>
#include <lightmetrica/intersection.h>
#include <lightmetrica/emitter.h>
#include <lightmetrica/sensor.h>
#include <lightmetrica/surfacegeometry.h>
#include <lightmetrica/primitive.h>
#include <lightmetrica/scheduler.h>
#include <lightmetrica/renderutils.h>
#include <lightmetrica/detail/photonmap.h>

LM_NAMESPACE_BEGIN

/*!
    \brief Progressive photon mapping renderer.

    - Implements progressive photon mapping
    - Based on the simplified implementation by Toshiya Hachisuka
      http://www.ci.i.u-tokyo.ac.jp/~hachisuka/smallppm_exp.cpp
*/
class Renderer_PPM final : public Renderer
{
public:

    LM_IMPL_CLASS(Renderer_PPM, Renderer);

public:

    LM_IMPL_F(Initialize) = [this](const PropertyNode* prop) -> bool
    {
        maxNumVertices_ = prop->Child("max_num_vertices")->As<int>();
        numSamples_     = prop->ChildAs<long long>("num_samples", 100000L);
        numPhotonPass_  = prop->ChildAs<long long>("num_photon_pass", 1000L);
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

        #pragma region Function to trace subpath

        struct PathVertex
        {
            int type;
            SurfaceGeometry geom;
            const Primitive* primitive = nullptr;
        };

        const auto TraceSubpath = [this, &scene](Random* rng, TransportDirection transDir, const std::function<bool(const PathVertex&)>& processPathVertexFunc) -> void
        {
            Vec3 initWo;
            PathVertex pv, ppv;
            SPD throughput(1_f);
            for (int step = 0; maxNumVertices_ == -1 || step < maxNumVertices_; step++)
            {
                if (step == 0)
                {
                    #pragma region Sample initial vertex

                    PathVertex v;

                    // Sample an emitter
                    v.type = transDir == TransportDirection::LE ? SurfaceInteractionType::L : SurfaceInteractionType::E;
                    v.primitive = scene->SampleEmitter(v.type, rng->Next());

                    // Sample a position on the emitter and initial ray direction
                    v.primitive->emitter->SamplePositionAndDirection(rng->Next2D(), rng->Next2D(), v.geom, initWo);

                    // Add a vertex
                    pv = v;

                    #pragma endregion
                }
                else
                {
                    #pragma region Sample a vertex with PDF with BSDF

                    // Sample a next direction
                    Vec3 wi;
                    Vec3 wo;
                    if (step == 1)
                    {
                        wi = Vec3();
                        wo = initWo;
                    }
                    else
                    {
                        wi = Math::Normalize(ppv.geom.p - pv.geom.p);
                        pv.primitive->surface->SampleDirection(rng->Next2D(), rng->Next(), pv.type, pv.geom, wi, wo);
                    }
                    const auto f = pv.primitive->surface->EvaluateDirection(pv.geom, pv.type, wi, wo, transDir, false);
                    if (f.Black())
                    {
                        break;
                    }

                    // Intersection query
                    Ray ray = { pv.geom.p, wo };
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

                    #pragma region Process path vertex

                    PathVertex v;
                    v.geom = isect.geom;
                    v.primitive = isect.primitive;
                    v.type = isect.primitive->surface->Type() & ~SurfaceInteractionType::Emitter;
                    if (!processPathVertexFunc(v))
                    {
                        break;
                    }

                    #pragma endregion

                    // --------------------------------------------------------------------------------

                    #pragma region Path termination

                    // TODO: replace it with efficient one
                    const Float rrProb = 0.5_f;
                    if (rng->Next() > rrProb)
                    {
                        break;
                    }

                    #pragma endregion

                    // --------------------------------------------------------------------------------

                    #pragma region Update information

                    ppv = pv;
                    pv = v;

                    #pragma endregion
                }
            }
        };

        #pragma endregion

        // --------------------------------------------------------------------------------

        #pragma region Collect measumrement points

        std::vector<PathVertex> mp;
        for (long long sample = 0; sample < numSamples_; sample++)
        {
            TraceSubpath(&initRng, TransportDirection::EL, [&](const PathVertex& v) -> bool
            {
                if ((v.type & SurfaceInteractionType::D) > 0 || (v.type & SurfaceInteractionType::G) > 0)
                {
                    // Record the measurement point, and terminate the path
                    mp.push_back(v);
                    return false;
                }

                // If the intersected point is specular surface, continue to trace the path
                return true;
            });
        }
        
        #pragma endregion

        // --------------------------------------------------------------------------------

#if 0
        // Output the measurement points to the file
        {
            std::ofstream ofs("mp.dat");
            for (const auto& v : mp)
            {
                const auto& p = v.geom.p;
                ofs << boost::str(boost::format("%.15f %.15f %.15f") % p.x % p.y % p.z) << std::endl;
            }
        }
#endif
        
        // --------------------------------------------------------------------------------

        #pragma region Photon scattering pass

        // Create photon map
        

        #pragma endregion

        // --------------------------------------------------------------------------------


    };

private:

    int maxNumVertices_;
    long long numSamples_;      // Number of measurement points
    long long numPhotonPass_;   // Number of photon scattering passes

};

LM_COMPONENT_REGISTER_IMPL(Renderer_PPM, "renderer::ppm");

LM_NAMESPACE_END
