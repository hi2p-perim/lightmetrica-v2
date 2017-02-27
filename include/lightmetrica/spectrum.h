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

#include <lightmetrica/macros.h>
#include <lightmetrica/math.h>
#include <lightmetrica/probability.h>

LM_NAMESPACE_BEGIN

/*!
    \addtogroup core
    \{
*/

// TODO: Make configurable
// #define LM_USE_SPECTRUM_SINGLE
#define LM_USE_SPECTRUM_RGB
// #define LM_USE_SPECTRUM_MULTI

#ifdef LM_USE_SPECTRUM_SINGLE
    #define LM_SPECTRUM_SINGLE 1
#endif
#ifdef LM_USE_SPECTRUM_RGB
    #define LM_SPECTRUM_RGB 1
#endif
#ifdef LM_USE_SPECTRUM_MULTI
    #define LM_SPECTRUM_MULTI 1
    #ifndef LM_SPECTRUM_SPD_N
        #error "Missing number of elements in SPD"
    #endif
#endif

/*!
    Base template type for spectrum class.

    This class is an abstract of discrete spectrum power distribution.
    utilized in various class (e.g., BSDF) for representing basic quantities of light transport.

    TODO: Implement general version.

    \tparam N Number of elements in the spectrum.
*/
template <int N>
struct DiscreteSPD
{
    // Values for the spectrum
    Float p[N]{};
};

/*!
    Specialization for the SPD representing RGB colors.
*/
template <>
struct DiscreteSPD<3>
{
    using T = DiscreteSPD<3>;
    Vec3 v;

    DiscreteSPD() {}
    DiscreteSPD(Float s) : v(s) {}
    DiscreteSPD(const Vec3& v) : v(v) {}

    auto ToRGB() const -> Vec3 { return v; }
    static auto FromRGB(const Vec3& rgb) -> T { T s; s.v = rgb; return s; }

    auto Black() const -> bool { return Math::IsZero(v); }

    LM_INLINE auto operator+=(const DiscreteSPD<3>& spd) -> T& { v += spd.v; return *this; }
    LM_INLINE auto operator*=(const DiscreteSPD<3>& spd) -> T& { v *= spd.v; return *this; }
    LM_INLINE auto operator*=(Float s) -> T& { v *= s; return *this; }
    LM_INLINE auto operator/=(Float s) -> T& { v /= s; return *this; }
};

LM_INLINE auto operator+(const DiscreteSPD<3>& spd1, const DiscreteSPD<3>& spd2) -> DiscreteSPD<3>
{
    return DiscreteSPD<3>(spd1.v + spd2.v);
}

LM_INLINE auto operator-(const DiscreteSPD<3>& spd1, const DiscreteSPD<3>& spd2) -> DiscreteSPD<3>
{
    return DiscreteSPD<3>(spd1.v - spd2.v);
}

LM_INLINE auto operator*(const DiscreteSPD<3>& spd1, const DiscreteSPD<3>& spd2) -> DiscreteSPD<3>
{
    return DiscreteSPD<3>(spd1.v * spd2.v);
}

LM_INLINE auto operator/(const DiscreteSPD<3>& spd1, const DiscreteSPD<3>& spd2) -> DiscreteSPD<3>
{
    return DiscreteSPD<3>(spd1.v / spd2.v);
}

LM_INLINE auto operator/(const DiscreteSPD<3>& spd, Float s) -> DiscreteSPD<3>
{
    return DiscreteSPD<3>(spd.v / s);
}

LM_INLINE auto operator/(const DiscreteSPD<3>& spd, const PDFVal& p) -> DiscreteSPD<3>
{
    assert(p.v != 0_f || (p.v == 0_f && spd.Black()));
    if (spd.Black()) return DiscreteSPD<3>();
    return spd / p.v;
}

LM_INLINE auto operator/(Float spd, const PDFVal& p) -> Float
{
    assert(p.v != 0_f || (p.v == 0_f && spd == 0_f));
    if (spd == 0_f) return 0_f;
    return spd / p.v;
}

#if LM_SPECTRUM_MULTI
    using SPD = DiscreteSPD<LM_SPECTRUM_SPD_N>;
#elif LM_SPECTRUM_RGB
    using SPD = DiscreteSPD<3>;
#elif LM_SPECTRUM_SINGLE
    using SPD = DiscreteSPD<1>;
#endif

//! \}

LM_NAMESPACE_END

