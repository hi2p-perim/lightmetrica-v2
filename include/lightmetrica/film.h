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

class Film : public Asset
{
public:

    LM_INTERFACE_CLASS(Film, Asset, 8);

public:

    Film() = default;
    LM_DISABLE_COPY_AND_MOVE(Film);

public:

    LM_INTERFACE_F(0, Width, int());
    LM_INTERFACE_F(1, Height, int());
    LM_INTERFACE_F(2, Splat, void(const Vec2& rasterPos, const SPD& v));
    LM_INTERFACE_F(3, SetPixel, void(int x, int y, const SPD& v));
    LM_INTERFACE_F(4, Save, bool(const std::string&));
    LM_INTERFACE_F(5, Accumulate, void(const Film* film));
    LM_INTERFACE_F(6, Rescale, void(Float w));
    LM_INTERFACE_F(7, Clear, void());

};

LM_NAMESPACE_END
