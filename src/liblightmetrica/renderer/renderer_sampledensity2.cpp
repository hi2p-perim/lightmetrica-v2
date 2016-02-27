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
#include <lightmetrica/scene.h>
#include <lightmetrica/film.h>
#include <lightmetrica/property.h>
#include <lightmetrica/scheduler.h>
#include <lightmetrica/emitter.h>
#include <lightmetrica/primitive.h>
#include <lightmetrica/random.h>

LM_NAMESPACE_BEGIN

class Renderer_SampleDensity2 final : public Renderer
{
public:

    LM_IMPL_CLASS(Renderer_SampleDensity2, Renderer);

public:

    Renderer_SampleDensity2()
        : sched_(ComponentFactory::Create<Scheduler>())
    {}

public:

    LM_IMPL_F(Initialize) = [this](const PropertyNode* prop) -> bool
    {
        sched_->Load(prop);
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

        sched_->Process(scene, film, &initRng, [this](const Scene* scene, Film* film, Random* rng)
        {
            const auto* E = scene->Sensor();

            SurfaceGeometry geomE;
            E->SamplePosition(Vec2(0.5), Vec2(0.5), geomE);    // Fix

            // Sample direction
            Vec3 wo;
            E->SampleDirection(rng->Next2D(), rng->Next(), SurfaceInteraction::E, geomE, Vec3(), wo);

            // To local coordinates
            const auto localWo = geomE.ToLocal * wo;
            
            Vec2 rasterPos(localWo.x, localWo.y);
            rasterPos = (rasterPos + Vec2(1_f)) * .5_f;
            film->Splat(rasterPos, SPD(1));
        });
    };

private:

    Scheduler::UniquePtr sched_;

};

LM_COMPONENT_REGISTER_IMPL(Renderer_SampleDensity2, "renderer::sampledensity2");

LM_NAMESPACE_END
