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

#if LM_COMPILER_CLANG
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wundefined-internal"
#endif

LM_NAMESPACE_BEGIN

namespace
{

    #if LM_COMPIER_MSVC

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

    #else

    template<class Tag>
    struct MetaCounter {
    using size_type = std::size_t;

    template<size_type N>
    struct ident {
        friend constexpr size_type adl_lookup (ident<N>);
        static constexpr size_type value = N;
    };

    template<class Ident>
    struct writer {
      friend constexpr size_type adl_lookup (Ident) {
        return Ident::value;
      }

      static constexpr size_type value = Ident::value;
    };

    template<size_type N, int = adl_lookup (ident<N> {})>
    static constexpr size_type value_reader (int, ident<N>) {
      return N;
    }

    template<size_type N>
    static constexpr size_type value_reader (float, ident<N>, size_type R = value_reader (0, ident<N-1> ())) {
      return R;
    }

    static constexpr size_type value_reader (float, ident<0>) {
      return 0;
    }

    template<size_type Max = 64>
    static constexpr size_type Value (size_type R = value_reader (0, ident<Max> {})) {
      return R;
    }

    template<size_type N = 1, class H = MetaCounter>
    static constexpr size_type Next (size_type R = writer<ident<N + H::Value ()>>::value) {
      return R;
    }
  };

    #endif

}

LM_NAMESPACE_END

#if LM_COMPILER_CLANG
#pragma clang diagnostic pop
#endif

