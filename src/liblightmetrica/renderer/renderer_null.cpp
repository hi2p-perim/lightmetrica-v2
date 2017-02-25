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
#include <lightmetrica/sensor.h>
#include <lightmetrica/primitive.h>

LM_NAMESPACE_BEGIN

class Renderer_Null final : public Renderer
{
public:

    LM_IMPL_CLASS(Renderer_Null, Renderer);

public:

    LM_IMPL_F(Initialize) = [this](const PropertyNode* prop) -> bool
    {
        c_ = prop->ChildAs("c", Vec3(1_f));
        return true;
    };

    LM_IMPL_F(Render) = [this](const Scene* scene_, Random* initRng, const std::string& outputPath) -> void
    {
        // Do nothing. Just output blank image.
        const auto* scene = static_cast<const Scene3*>(scene_);
        auto* film = static_cast<const Sensor*>(scene->GetSensor()->emitter)->GetFilm();
        for (int y = 0; y < film->Height(); y++)
        {
            for (int x = 0; x < film->Width(); x++)
            {
                film->SetPixel(x, y, SPD::FromRGB(c_));
            }
        }

        // --------------------------------------------------------------------------------

        #pragma region Save image
        {
            LM_LOG_INFO("Saving image");
            LM_LOG_INDENTER();
            film->Save(outputPath);
        }
        #pragma endregion
    };

private:

    Vec3 c_;

};

LM_COMPONENT_REGISTER_IMPL(Renderer_Null, "renderer::nulltype");

LM_NAMESPACE_END
