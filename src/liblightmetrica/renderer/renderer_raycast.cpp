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

#include <lightmetrica/renderer.h>
#include <lightmetrica/scene.h>
#include <lightmetrica/film.h>
#include <lightmetrica/ray.h>

LM_NAMESPACE_BEGIN

class Renderer_Raycast : public Renderer
{
public:

    LM_IMPL_CLASS(Renderer_Raycast, Renderer);

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

                // Create a ray
                Ray ray;
                

                film->SetPixel(x, y, SPD());
            }

            const double progress = 100.0 * y / film->Height();
            LM_LOG_INPLACE(boost::str(boost::format("Progress: %.1f%%") % progress));
        }
        LM_LOG_INFO("Progress: 100.0%");
    };

};

LM_COMPONENT_REGISTER_IMPL(Renderer_Raycast, "raycast");

LM_NAMESPACE_END
