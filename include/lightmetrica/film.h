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

#pragma once

#include <lightmetrica/asset.h>
#include <lightmetrica/spectrum.h>

LM_NAMESPACE_BEGIN

/*!
    \defgroup film Film
    \brief An abstraction of the image films
    \ingroup asset
*/

/*!
    \brief Film.

    A base class of the films.
    The class is used to rendered images equipped with sensor.

    \ingroup film
*/
class Film : public Asset
{
public:

    LM_INTERFACE_CLASS(Film, Asset, 9);

public:

    Film() = default;
    LM_DISABLE_COPY_AND_MOVE(Film);

public:

    /*!
        \brief Width of the film.
        \return Width of the film.
    */
    LM_INTERFACE_F(0, Width, int());

    /*!
        \brief Height of the film.
        \return Height of the film.
    */
    LM_INTERFACE_F(1, Height, int());

    /*!
        \brief Accumulate the contribution to the raster position.
        This function accumulates (**splats**) the contribution `v` to the raster position `rasterPos`.
        \param rasterPos Raster position.
        \param contrb Contribution.
    */
    LM_INTERFACE_F(2, Splat, void(const Vec2& rasterPos, const SPD& v));

    /*!
        \brief Set the value of the pixel.
        This function set the pixel value specified by `v` to the screen coordinates (x,y).
        \param rasterPos Raster position.
        \param contrb Contribution.
    */
    LM_INTERFACE_F(3, SetPixel, void(int x, int y, const SPD& v));

    /*!
        \brief Save as image.
        Saves the film as image.
        If `path` is empty, the default path is used.
        \param path Path to the output image.
        \retval true Succeeded to save the image.
        \retval false Failed to save the image.
    */
    LM_INTERFACE_F(4, Save, bool(const std::string&));

    /*!
        \brief Accumulate the contribution to the entire film.
        This function accumulates the contribution of the other film.
        The other film must be same size and type.
        \param film Other film.
    */
    LM_INTERFACE_F(5, Accumulate, void(const Film* film));

    /*!
        \brief Rescale the pixel values by the constant weight.
        \param w Rescaling weight.
    */
    LM_INTERFACE_F(6, Rescale, void(Float w));

    /*!
        \brief Clear the film.
        Fills in the pixel values to zero.
    */
    LM_INTERFACE_F(7, Clear, void());

    ///! Computes pixel index from the raster position.
    LM_INTERFACE_F(8, PixelIndex, int(const Vec2& rasterPos));

};

LM_NAMESPACE_END
