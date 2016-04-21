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
#include <lightmetrica/detail/photonmap.h>

LM_NAMESPACE_BEGIN

class PhotonMap_Naive : public PhotonMap
{
public:

    LM_IMPL_CLASS(PhotonMap_Naive, PhotonMap);

public:

    virtual auto Build(const std::vector<Photon>& photons) -> void
    {
        photons_ = photons;
    }

    virtual auto CollectPhotons(const Vec3& p, int n, Float maxDist2, std::vector<Photon>& collected) const -> Float
    {
        collected.clear();

        const auto comp = [&](const Photon& p1, const Photon& p2)
        {
            return Math::Length2(p1.p - p) < Math::Length2(p2.p - p);
        };

        for (const auto& photon : photons_)
        {
            if (Math::Length2(photon.p - p) < maxDist2)
            {
                if ((int)(collected.size()) < n)
                {
                    collected.push_back(photon);
                    if ((int)(collected.size()) == n)
                    {
                        // Create heap
                        std::make_heap(collected.begin(), collected.end(), comp);
                        maxDist2 = Math::Length2(collected.front().p - p);
                    }
                }
                else
                {
                    // Update heap
                    std::pop_heap(collected.begin(), collected.end(), comp);
                    collected.back() = photon;
                    std::push_heap(collected.begin(), collected.end(), comp);
                    maxDist2 = Math::Length2(collected.front().p - p);
                }
            }
        }

        return maxDist2;
    }

private:

    std::vector<Photon> photons_;

};

LM_COMPONENT_REGISTER_IMPL(PhotonMap_Naive, "photonmap::naive");

LM_NAMESPACE_END
