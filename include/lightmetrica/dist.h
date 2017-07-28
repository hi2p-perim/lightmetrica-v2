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

#include <lightmetrica/math.h>
#include <vector>
#include <algorithm>

LM_NAMESPACE_BEGIN

/*!
    \brief Discrete 1D distribution.
    \ingroup math
*/
class Distribution1D
{
public:

    Distribution1D() { Clear(); }

public:

    //! Add an value
    auto Add(Float v) -> void
    {
        cdf.push_back(cdf.back() + v);
    }

    //! Normalize the histogram
    auto Normalize() -> void
    {
        const Float sum = cdf.back();
        const Float invSum = 1_f / sum;
        for (auto& v : cdf)
        {
            v *= invSum;
        }
    }

    //! Sample from the distribution
    auto Sample(Float u) const -> int
    {
        int v = static_cast<int>(std::upper_bound(cdf.begin(), cdf.end(), u) - cdf.begin()) - 1;
        return Math::Clamp<int>(v, 0, static_cast<int>(cdf.size()) - 2);
    }

    //! Sample from the distribution reusing a random variable
    auto SampleReuse(Float u, Float& u2) const -> int
    {
        int v = static_cast<int>(std::upper_bound(cdf.begin(), cdf.end(), u) - cdf.begin()) - 1;
        int i = Math::Clamp<int>(v, 0, static_cast<int>(cdf.size()) - 2);
        u2 = (u - cdf[i]) / (cdf[i + 1] - cdf[i]);
        return i;
    }

    //! Evaluate distribution
    auto EvaluatePDF(int i) const -> Float
    {
        return (i < 0 || i + 1 >= static_cast<int>(cdf.size())) ? 0 : cdf[i + 1] - cdf[i];
    }

    auto EvaluateCDF(int i) const -> Float
    {
        return cdf[i];
    }

    //! Clear distribution
    auto Clear() -> void
    {
        cdf.clear();
        cdf.push_back(0);
    }

    //! Check if the distribution is empty
    auto Empty() const -> bool
    {
        return cdf.size() == 1;
    }

    auto Sum() const -> Float
    {
        return cdf.back();
    }

public:

    std::vector<Float> cdf;

};

LM_NAMESPACE_END
