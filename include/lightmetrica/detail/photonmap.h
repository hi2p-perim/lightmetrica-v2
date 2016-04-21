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

#include <lightmetrica/component.h>
#include <lightmetrica/spectrum.h>

LM_NAMESPACE_BEGIN

/*!
    \addtogroup renderer
    \{
*/

///! Photon
struct Photon : public SIMDAlignedType
{
    Vec3 p;             //!< Position on the surface
    SPD throughput;     //!< Current throughput
    Vec3 wi;            //!< Incident ray direction
    int numVertices;    //!< Number of path vertices of the light path that the photon is generated
};

///! Base class of photon map
struct PhotonMap : public Component
{
    LM_INTERFACE_CLASS(PhotonMap, Component, 0);

    /*!
        \brief Build the photon map

        Build the photon map with the underlying spatial data structure
        utilizing the given vector of photons.
    */
    virtual auto Build(const std::vector<Photon>& photons) -> void = 0;

    /*!
        \brief Collect photons

        Collect at most `n` nearest photons within the distance sqrt(maxDist2) from `p`.
        The collected photons are stored into `collected` ordered from the most distant photons.

        \param p            Gather point
        \param n            Number of maximum photons to be collected
        \param maxDist2     Maximum distance from the gather point
        \param collected    Collected photons
        \return             Maximum distance from the gather point for the collected photons
    */
    virtual auto CollectPhotons(const Vec3& p, int n, Float maxDist2, std::vector<Photon>& collected) const -> Float = 0;
};

//! \}

LM_NAMESPACE_END
