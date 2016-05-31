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
#include <lightmetrica/film.h>
#include <lightmetrica/property.h>
#include <lightmetrica/logger.h>
#include <lightmetrica/enum.h>
#include <FreeImage.h>

LM_NAMESPACE_BEGIN

namespace
{
    void FreeImageErrorHandler(FREE_IMAGE_FORMAT fif, const char *message)
    {
        if (fif != FIF_UNKNOWN)
        {
            LM_LOG_ERROR(std::string(FreeImage_GetFormatFromFIF(fif)) + " Format");
        }
        LM_LOG_ERROR(message);
    }

    bool SaveImage(const std::string& path, const std::vector<Vec3>& film, int width, int height)
    {
        FreeImage_SetOutputMessage(FreeImageErrorHandler);

        // --------------------------------------------------------------------------------

        #pragma region Check & create output directory

        {
            const auto parent = boost::filesystem::path(path).parent_path();
            if (!boost::filesystem::exists(parent) && parent != "")
            {
                LM_LOG_INFO("Creating directory : " + parent.string());
                if (!boost::filesystem::create_directories(parent))
                {
                    LM_LOG_WARN("Failed to create output directory : " + parent.string());
                    return false;
                }
            }
        }

        #pragma endregion

        // --------------------------------------------------------------------------------

        #pragma region Save image

        {
            boost::filesystem::path fsPath(path);
            if (fsPath.extension() == ".hdr" || fsPath.extension() == ".exr")
            {
                #pragma region HDR

                FIBITMAP* fibitmap = FreeImage_AllocateT(FIT_RGBF, width, height);
                if (!fibitmap)
                {
                    LM_LOG_ERROR("Failed to allocate bitmap");
                    return false;
                }

                for (int y = 0; y < height; y++)
                {
                    FIRGBF* bits = (FIRGBF*)FreeImage_GetScanLine(fibitmap, y);
                    for (int x = 0; x < width; x++)
                    {
                        const int i = y * width + x;
                        //bits[x].red   = (float)(Math::Clamp(film[i][0], 0_f, 1_f));
                        //bits[x].green = (float)(Math::Clamp(film[i][1], 0_f, 1_f));
                        //bits[x].blue  = (float)(Math::Clamp(film[i][2], 0_f, 1_f));
                        bits[x].red   = (float)(Math::Max(film[i][0], 0_f));
                        bits[x].green = (float)(Math::Max(film[i][1], 0_f));
                        bits[x].blue  = (float)(Math::Max(film[i][2], 0_f));
                    }
                }

                if (!FreeImage_Save(fsPath.extension() == ".hdr" ? FIF_HDR : FIF_EXR, fibitmap, path.c_str(), HDR_DEFAULT))
                {
                    LM_LOG_ERROR("Failed to save image : " + path);
                    FreeImage_Unload(fibitmap);
                    return false;
                }

                LM_LOG_INFO("Successfully saved to " + path);
                FreeImage_Unload(fibitmap);

                #pragma endregion
            }
            else if (fsPath.extension() == ".png")
            {
                #pragma region PNG

                FIBITMAP* tonemappedBitmap = FreeImage_Allocate(width, height, 24, FI_RGBA_RED_MASK, FI_RGBA_GREEN_MASK, FI_RGBA_BLUE_MASK);
                if (!tonemappedBitmap)
                {
                    LM_LOG_ERROR("Failed to allocate bitmap");
                    return false;
                }

                const double Exp = 1.0 / 2.2;
                const int Bytespp = 3;
                for (int y = 0; y < height; y++)
                {
                    BYTE* bits = FreeImage_GetScanLine(tonemappedBitmap, y);
                    for (int x = 0; x < width; x++)
                    {
                        int idx = y * width + x;
                        bits[FI_RGBA_RED]   = (BYTE)(Math::Clamp((int)(Math::Pow((double)(film[idx][0]), Exp) * 255_f), 0, 255));
                        bits[FI_RGBA_GREEN] = (BYTE)(Math::Clamp((int)(Math::Pow((double)(film[idx][1]), Exp) * 255_f), 0, 255));
                        bits[FI_RGBA_BLUE]  = (BYTE)(Math::Clamp((int)(Math::Pow((double)(film[idx][2]), Exp) * 255_f), 0, 255));
                        bits += Bytespp;
                    }
                }

                if (!FreeImage_Save(FIF_PNG, tonemappedBitmap, path.c_str(), PNG_DEFAULT))
                {
                    LM_LOG_ERROR("Failed to save image : " + path);
                    FreeImage_Unload(tonemappedBitmap);
                    return false;
                }

                LM_LOG_INFO("Successfully saved to " + path);
                FreeImage_Unload(tonemappedBitmap);

                #pragma endregion
            }
            else
            {
                LM_LOG_ERROR("Invalid extension: " + fsPath.extension().string());
                return false;
            }
        }

        #pragma endregion

        // --------------------------------------------------------------------------------

        return true;
    }
}

enum class HDRImageType
{
    RadianceHDR,
    OpenEXR,
    PNG,
};

const std::string HDRImageType_String[] =
{
    "radiancehdr",
    "openexr",
    "png",
};

LM_ENUM_TYPE_MAP(HDRImageType);

class Film_HDR final : public Film
{
public:

    LM_IMPL_CLASS(Film_HDR, Film);

public:

    LM_IMPL_F(Load) = [this](const PropertyNode* prop, Assets* assets, const Primitive* primitive) -> bool
    {
        width_  = prop->Child("w")->As<int>();
        height_ = prop->Child("h")->As<int>();
        if (prop->Child("type"))
        {
            type_ = LM_STRING_TO_ENUM(HDRImageType, prop->Child("type")->As<std::string>());
        }

        data_.assign(width_ * height_, Vec3());

        return true;
    };

    LM_IMPL_F(Clone) = [this](Clonable* o) -> void
    {
        auto* film = static_cast<Film_HDR*>(o);
        film->width_ = width_;
        film->height_ = height_;
        film->data_ = data_;
    };

    LM_IMPL_F(Width) = [this]() -> int
    {
        return width_;
    };

    LM_IMPL_F(Height) = [this]() -> int
    {
        return height_;
    };

    LM_IMPL_F(Splat) = [this](const Vec2& rasterPos, const SPD& v) -> void
    {
        const int pX = Math::Clamp((int)(rasterPos.x * Float(width_)), 0, width_ - 1);
        const int pY = Math::Clamp((int)(rasterPos.y * Float(height_)), 0, height_ - 1);
        data_[pY * width_ + pX] += v.ToRGB();
    };

    LM_IMPL_F(SetPixel) = [this](int x, int y, const SPD& v) -> void
    {
        #if LM_DEBUG_MODE
        if (x < 0 || width_ <= x || y < 0 || height_ <= y)
        {
            LM_LOG_ERROR("Out of range");
            return;
        }
        #endif

        // Convert to RGB and record to data
        data_[y * width_ + x] = v.ToRGB();
    };

    LM_IMPL_F(Save) = [this](const std::string& path) -> bool
    {
        #if 0
        boost::filesystem::path p(path);
        {
            // Check if extension is already contained in `path`
            const auto filename = p.filename();
            if (filename.has_extension())
            {
                LM_LOG_INFO("Extension is found '" + p.extension().string() + "'. Replaced.");
            }

            // Replace or add an extension according to the type
            if (type_ == HDRImageType::RadianceHDR)
            {
                p.replace_extension(".hdr");
            }
            else if (type_ == HDRImageType::OpenEXR)
            {
                p.replace_extension(".exr");
            }
        }

        return SaveImage(p.string(), data_, width_, height_);
        #endif

        auto p = path;
        if (type_ == HDRImageType::RadianceHDR)
        {
            p += ".hdr";
        }
        else if (type_ == HDRImageType::OpenEXR)
        {
            p += ".exr";
        }
        else if (type_ == HDRImageType::PNG)
        {
            p += ".png";
        }

        return SaveImage(p, data_, width_, height_);
    };

    LM_IMPL_F(Accumulate) = [this](const Film* film_) -> void
    {
        assert(implName == film_->implName);                            // Internal type must be same
        const auto* film = static_cast<const Film_HDR*>(film_);
        assert(width_ == film->width_ && height_ == film->height_);     // Image size must be same
        std::transform(data_.begin(), data_.end(), film->data_.begin(), data_.begin(), std::plus<Vec3>());
    };

    LM_IMPL_F(Rescale) = [this](Float w) -> void
    {
        for (auto& v : data_) { v *= w; }
    };

    LM_IMPL_F(Clear) = [this]() -> void
    {
        data_.assign(width_ * height_, Vec3());
    };

    LM_IMPL_F(PixelIndex) = [this](const Vec2& rasterPos) -> int
    {
        const int pX = Math::Clamp((int)(rasterPos.x * Float(width_)), 0, width_ - 1);
        const int pY = Math::Clamp((int)(rasterPos.y * Float(height_)), 0, height_ - 1);
        return pY * width_ + pX;
    };

private:

    int width_;
    int height_;
    HDRImageType type_ = HDRImageType::RadianceHDR;
    std::vector<Vec3> data_;
    
};

LM_COMPONENT_REGISTER_IMPL(Film_HDR, "film::hdr");

LM_NAMESPACE_END
