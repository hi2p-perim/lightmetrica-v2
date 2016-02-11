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

#include <lightmetrica/lightmetrica.h>

LM_NAMESPACE_BEGIN

class Renderer_Normal final : public Renderer
{
public:

    LM_IMPL_CLASS(Renderer_Normal, Renderer);

public:

    LM_IMPL_F(Initialize) = [this](const PropertyNode* prop) -> bool
    {
        return true;
    };

    LM_IMPL_F(Render) = [this](const Scene* scene, Film* film) -> void
    {
        const int w = film->Width();
        const int h = film->Height();

        for (int y = 0; y < h; y++)
        {
            for (int x = 0; x < w; x++)
            {
                // Raster position
                Vec2 rasterPos((Float(x) + 0.5_f) / Float(w), (Float(y) + 0.5_f) / Float(h));

                // Position and direction of a ray
                const auto* E = scene->Sensor()->emitter;
                SurfaceGeometry geomE;
                E->SamplePosition(Vec2(), geomE);
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
                    continue;
                }

                // Set color to the pixel
                film->SetPixel(x, y, SPD::FromRGB(Vec3(
                    Math::Abs(isect.geom.sn.x),
                    Math::Abs(isect.geom.sn.y),
                    Math::Abs(isect.geom.sn.z))));
            }
        }
    };

};

LM_COMPONENT_REGISTER_IMPL(Renderer_Normal, "renderer::normal");

LM_NAMESPACE_END
