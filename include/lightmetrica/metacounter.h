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

namespace
{

    /*
        Compile-time counter.

        Based on meta_counter.hpp by Filip Roséen
        http://b.atch.se/posts/constexpr-meta-container
    */
    template<class Tag>
    struct MetaCounter
    {
        using SizeType = int;

        template<SizeType N>
        struct Ident
        {
            friend constexpr SizeType ADLLookup(Ident<N>);
            static constexpr SizeType value = N;
        };

        template<class IdentType>
        struct Writer
        {
            friend constexpr SizeType ADLLookup(IdentType)
            {
                return IdentType::value;
            }

            static constexpr SizeType value = IdentType::value;
        };

        template<SizeType N, class = char[noexcept(ADLLookup(Ident<N>())) ? +1 : -1]>
        static constexpr SizeType ValueReader(int, Ident<N>)
        {
            return N;
        }

        template<SizeType N>
        static constexpr SizeType ValueReader(float, Ident<N>, SizeType R = ValueReader(0, Ident<N - 1>()))
        {
            return R;
        }

        static constexpr SizeType ValueReader(float, Ident<0>)
        {
            return 0;
        }

        template<SizeType Max = 64>
        static constexpr SizeType Value(SizeType R = ValueReader(0, Ident<Max>()))
        {
            return R;
        }

        template<SizeType N = 1, class H = MetaCounter, int C = H::Value()>
        static constexpr SizeType Next(SizeType R = Writer<Ident<N + C>>::value)
        {
            return R;
        }
    };

}

LM_NAMESPACE_END
