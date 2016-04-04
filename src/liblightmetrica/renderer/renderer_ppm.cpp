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

LM_NAMESPACE_BEGIN

#pragma region Importance map

struct HitPoint
{
    
};

#pragma endregion

// --------------------------------------------------------------------------------

/*!
    \brief Progressive photon mapping renderer.
    Implements progressive photon mapping.
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

        const auto TraceSubpath = [&](Random* rng, TransportDirection transDir, std::function<void()>& surfaceInteractionFunc) -> void
        {
            Vec3 initWo;
            for (int step = 0; maxPathVertices == -1 || step < maxPathVertices; step++)
            {
                if (step == 0)
                {
                    #pragma region Sample initial vertex

                    PathVertex sv;

                    // Sample an emitter
                    sv.type = transDir == TransportDirection::LE ? SurfaceInteractionType::L : SurfaceInteractionType::E;
                    sv.primitive = scene->SampleEmitter(sv.type, rng->Next());

                    // Sample a position on the emitter and initial ray direction
                    sv.primitive->emitter->SamplePositionAndDirection(rng->Next2D(), rng->Next2D(), sv.geom, initWo);

                    // Add a vertex
                    SubpathVertex v;
                    v.sv = sv;
                    vertices.push_back(v);

                    #pragma endregion
                }
                else
                {
                    #pragma region Sample a vertex with PDF with BSDF

                    const auto sv = [&]() -> boost::optional<PathVertex>
                    {
                        // Previous & two before vertex
                        const auto* pv = &vertices.back().sv.get();
                        const auto* ppv = vertices.size() > 1 ? &vertices[vertices.size() - 2].sv.get() : nullptr;

                        // Sample a next direction
                        Vec3 wo;
                        const auto wi = ppv ? Math::Normalize(ppv->geom.p - pv->geom.p) : Vec3();
                        if (step == 1)
                        {
                            wo = initWo;
                        }
                        else
                        {
                            pv->primitive->surface->SampleDirection(rng->Next2D(), rng->Next(), pv->type, pv->geom, wi, wo);
                        }
                        const auto f = pv->primitive->surface->EvaluateDirection(pv->geom, pv->type, wi, wo, transDir, false);
                        if (f.Black())
                        {
                            return boost::none;
                        }

                        // Intersection query
                        Ray ray = { pv->geom.p, wo };
                        Intersection isect;
                        if (!scene->Intersect(ray, isect))
                        {
                            return boost::none;
                        }

                        // Create vertex
                        PathVertex v;
                        v.geom = isect.geom;
                        v.primitive = isect.primitive;
                        v.type = isect.primitive->surface->Type() & ~SurfaceInteractionType::Emitter;

                        return v;
                    }();

                    #pragma endregion

                    // --------------------------------------------------------------------------------

                    #pragma region Sample a vertex with direct emitter sampling

                    const auto direct = [&]() -> boost::optional<PathVertex>
                    {
                        PathVertex v;
                        const auto& pv = vertices.back().sv;

                        // Sample a emitter
                        v.type = transDir == TransportDirection::LE ? SurfaceInteractionType::E : SurfaceInteractionType::L;
                        v.primitive = scene->SampleEmitter(v.type, rng->Next());

                        // Sample a position on the emitter
                        v.primitive->emitter->SamplePositionGivenPreviousPosition(rng->Next2D(), pv->geom, v.geom);

                        // Check visibility
                        if (!scene->Visible(pv->geom.p, v.geom.p))
                        {
                            return boost::none;
                        }

                        return v;
                    }();

                    #pragma endregion

                    // --------------------------------------------------------------------------------

                    #pragma region Add a vertex

                    if (sv || direct)
                    {
                        SubpathVertex v;
                        v.sv = sv;
                        v.direct = direct;
                        vertices.push_back(v);
                    }

                    #pragma endregion

                    // --------------------------------------------------------------------------------

                    #pragma region Path termination

                    if (!sv)
                    {
                        break;
                    }

                    if (sv->geom.infinite)
                    {
                        break;
                    }

                    // TODO: replace it with efficient one
                    const Float rrProb = 0.5_f;
                    if (rng->Next() > rrProb)
                    {
                        break;
                    }

                    #pragma endregion
                }
            }
        };

        #pragma endregion

        // --------------------------------------------------------------------------------

        #pragma region Collect measumrement points

        for (long long sample = 0; sample < numSamples_; sample++)
        {
            
        }

        #pragma endregion

        // --------------------------------------------------------------------------------

        #pragma region Create acceleration structure from a set of measurement points

        

        #pragma endregion

        // --------------------------------------------------------------------------------


    };

private:

    int maxNumVertices_;
    long long numSamples_;

};

LM_COMPONENT_REGISTER_IMPL(Renderer_PPM, "renderer::ppm");

LM_NAMESPACE_END
