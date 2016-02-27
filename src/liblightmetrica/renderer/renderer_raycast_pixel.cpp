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
#include <lightmetrica/ray.h>
#include <lightmetrica/primitive.h>
#include <lightmetrica/surfacegeometry.h>
#include <lightmetrica/emitter.h>
#include <lightmetrica/intersection.h>
#include <lightmetrica/property.h>

LM_NAMESPACE_BEGIN

class Renderer_RaycastPixel final : public Renderer
{
public:

    LM_IMPL_CLASS(Renderer_RaycastPixel, Renderer);

public:

    LM_IMPL_F(Initialize) = [this](const PropertyNode* prop) -> bool
    {
        x_ = prop->ChildAs<int>("x", 0);
        y_ = prop->ChildAs<int>("y", 0);
        return true;
    };

    LM_IMPL_F(Render) = [this](const Scene* scene, Film* film) -> void
    {
        const int w = film->Width();
        const int h = film->Height();

        int x = x_;
        int y = h - y_;

        // Raster position
        Vec2 rasterPos((Float(x) + 0.5_f) / Float(w), (Float(y) + 0.5_f) / Float(h));

        // Position and direction of a ray
        const auto* E = scene->Sensor()->emitter;
        SurfaceGeometry geomE;
        E->SamplePosition(Vec2(), Vec2(), geomE);
        Vec3 wo;
        E->SampleDirection(rasterPos, 0_f, 0, geomE, Vec3(), wo);

        // Setup a ray
        Ray ray = { geomE.p, wo };

        // Intersection query
        Intersection isect;
        if (!scene->Intersect(ray, isect))
        {
            // No intersection -> black
            film->SetPixel(x, y, SPD());
            return;
        }

        // Set color to the pixel
        const auto c = Math::Abs(Math::Dot(isect.geom.sn, -ray.d));
        film->SetPixel(x, y, SPD(c));
    };

private:

    int x_;
    int y_;

};

LM_COMPONENT_REGISTER_IMPL(Renderer_RaycastPixel, "renderer::raycast_pixel");

LM_NAMESPACE_END
