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
#include <lightmetrica/texture.h>
#include <lightmetrica/property.h>
#include <FreeImage.h>

LM_NAMESPACE_BEGIN

class Texture_Bitmap final : public Texture
{
public:

    LM_IMPL_CLASS(Texture_Bitmap, Texture);

public:

    LM_IMPL_F(Load) = [this](const PropertyNode* prop, Assets* assets, const Primitive* primitive) -> bool
    {
        #pragma region Load params

        const auto localpath = prop->Child("path")->As<std::string>();
        const auto basepath = boost::filesystem::path(prop->Tree()->Path()).parent_path();
        const auto path = (basepath / localpath).string();

        #pragma endregion

        // --------------------------------------------------------------------------------

        #pragma region Load texture

        {
            // Try to deduce the file format by the file signature
            auto format = FreeImage_GetFileType(path.c_str(), 0);
            if (format == FIF_UNKNOWN)
            {
                // Try to deduce the file format by the extension
                format = FreeImage_GetFIFFromFilename(path.c_str());
                if (format == FIF_UNKNOWN)
                {
                    // Unknown image
                    LM_LOG_ERROR("Unknown image format");
                    return false;
                }
            }

            // Check the plugin capability
            if (!FreeImage_FIFSupportsReading(format))
            {
                LM_LOG_ERROR("Unsupported format");
                return false;
            }

            // Load image
            auto* fibitmap = FreeImage_Load(format, path.c_str(), 0);
            if (!fibitmap)
            {
                LM_LOG_ERROR("Failed to load an image " + path);
                return false;
            }

            // Width and height
            width_ = FreeImage_GetWidth(fibitmap);
            height_ = FreeImage_GetHeight(fibitmap);

            // Image type and bits per pixel (BPP)
            const auto type = FreeImage_GetImageType(fibitmap);
            const auto bpp = FreeImage_GetBPP(fibitmap);
            if (!(type == FIT_RGBF || type == FIT_RGBAF || (type == FIT_BITMAP && (bpp == 24 || bpp == 32))))
            {
                FreeImage_Unload(fibitmap);
                LM_LOG_ERROR("Unsupportted format");
                return false;
            }

            // Flip the loaded image
            // Note that in FreeImage loaded image is flipped from the beginning,
            // i.e., y axis is originated from bottom-left point and grows upwards.
            FreeImage_FlipVertical(fibitmap);

            // Read image data
            data_.clear();
            for (int y = 0; y < height_; y++)
            {
                if (type == FIT_RGBF)
                {
                    auto* bits = (FIRGBF*)FreeImage_GetScanLine(fibitmap, y);
                    for (int x = 0; x < width_; x++)
                    {
                        data_.emplace_back(bits[x].red);
                        data_.emplace_back(bits[x].green);
                        data_.emplace_back(bits[x].blue);
                    }
                }
                else if (type == FIT_RGBAF)
                {
                    auto* bits = (FIRGBAF*)FreeImage_GetScanLine(fibitmap, y);
                    for (int x = 0; x < width_; x++)
                    {
                        data_.emplace_back(bits[x].red);
                        data_.emplace_back(bits[x].green);
                        data_.emplace_back(bits[x].blue);
                    }
                }
                else if (type == FIT_BITMAP)
                {
                    BYTE* bits = (BYTE*)FreeImage_GetScanLine(fibitmap, y);
                    for (int x = 0; x < width_; x++)
                    {
                        data_.push_back((float)(bits[FI_RGBA_RED]) / 255.0f);
                        data_.push_back((float)(bits[FI_RGBA_GREEN]) / 255.0f);
                        data_.push_back((float)(bits[FI_RGBA_BLUE]) / 255.0f);
                        bits += bpp / 8;
                    }
                }
            }

            FreeImage_Unload(fibitmap);

            // --------------------------------------------------------------------------------

            // Scale
            const auto scale = prop->ChildAs<Float>("scale", 1_f);
            for (auto& v : data_)
            {
                v *= scale;
            }

            // --------------------------------------------------------------------------------

            return true;
        }

        #pragma endregion

        return true;
    };

    LM_IMPL_F(PostLoad) = [this](const Scene* scene) -> bool
    {
        return true;
    };

    LM_IMPL_F(Evaluate) = [this](const Vec2& uv) -> Vec3
    {
        const int x = Math::Clamp<int>((int)(Math::Fract(uv.x) * width_), 0, width_ - 1);
        const int y = Math::Clamp<int>((int)(Math::Fract(uv.y) * height_), 0, height_ - 1);
        const int i = width_ * y + x;
        return Vec3(Float(data_[3 * i]), Float(data_[3 * i + 1]), Float(data_[3 * i + 2]));
    };

private:

    int width_;
    int height_;
    std::vector<float> data_;

};

LM_COMPONENT_REGISTER_IMPL(Texture_Bitmap, "texture::bitmap");

LM_NAMESPACE_END

