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

#include <lightmetrica/film.h>
#include <lightmetrica/property.h>

LM_NAMESPACE_BEGIN

class Film_HDR : public Film
{
public:

    LM_IMPL_CLASS(Film_HDR, Film);

public:

    LM_IMPL_F(Initialize) = [this](const PropertyNode*) -> bool { return true; };
    LM_IMPL_F(Save) = [this](const std::string&) -> bool { return false; };
    LM_IMPL_F(Width) = [this]() -> int { return width_; };
    LM_IMPL_F(Height) = [this]() -> int { return height_; };

private:

    int width_;
    int height_;
    
};

LM_COMPONENT_REGISTER_IMPL(Film_HDR, "hdr");

LM_NAMESPACE_END
