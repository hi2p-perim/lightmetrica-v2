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

LM_NAMESPACE_BEGIN

/*!
    \addtogroup core
    \{
*/

/*!
    \brief Generalized BSDF type.
    BSDF types of the surface interaction.
*/
namespace SurfaceInteraction
{
    enum Type
    {
        D = 1 << 0,         //!< Diffuse
        G = 1 << 1,         //!< Glossy
        S = 1 << 2,         //!< Specular
        L = 1 << 3,         //!< Light
        E = 1 << 4,         //!< Sensor
        BSDF = D | G | S,   //!< BSDF flag (D or G or S)
        Emitter = L | E,    //!< Emitter flag (L or E)
        None = 0
    };
};

/*!
    \brief Transport directioin.
    The direction of light transport.
*/
enum class TransportDirection
{
    LE,     //!< Light to sensor (L to E)
    EL      //!< Sensor to light (E to L)
};

//! \}

LM_NAMESPACE_END
