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

#include <lightmetrica/static.h>
#include <lightmetrica/math.h>

LM_NAMESPACE_BEGIN

//! \cond
class Random;
extern "C" LM_PUBLIC_API auto Random_Constructor(Random* p) -> void;
extern "C" LM_PUBLIC_API auto Random_Destructor(Random* p) -> void;
extern "C" LM_PUBLIC_API auto Random_SetSeed(Random* p, unsigned int seed) -> void;
extern "C" LM_PUBLIC_API auto Random_NextUInt(Random* p) -> unsigned int;
extern "C" LM_PUBLIC_API auto Random_Next(Random* p) -> double;
//! \endcond

/*!
    \brief Random number generator.

    As the underlying implementation, we uses SIMD-oriented Fast Mersenne Twister (SFMT)
    using an implementation by Mutsuo Saito and Makoto Matsumoto:
    http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/SFMT/

    \ingroup math
*/
class Random
{
public:

    Random() { LM_EXPORTED_F(Random_Constructor, this); }
    ~Random() { LM_EXPORTED_F(Random_Destructor, this); }
    LM_DISABLE_COPY_AND_MOVE(Random);

public:

    //! Set seed and initialize internal state.
    auto SetSeed(unsigned int seed) -> void { LM_EXPORTED_F(Random_SetSeed, this, seed); }

    //! Generate an uniform random number as unsigned int type.
    auto NextUInt() -> unsigned int { return LM_EXPORTED_F(Random_NextUInt, this); }

    //! Generate an uniform random number in [0,1].
    auto Next() -> Float { return Float(LM_EXPORTED_F(Random_Next, this)); }

    //! Generate uniform random numbers in [0,1]^2.
    LM_INLINE auto Next2D() -> Vec2
    {
        // Note : according to C++ standard, evaluation order of the arguments are undefined
        // so we avoid the implementation like Vec2(Next(), Next()).
        const auto u1 = Next();
        const auto u2 = Next();
        return Vec2(u1, u2);
    }

public:

    class Impl;
    Impl* p_;

};

LM_NAMESPACE_END
