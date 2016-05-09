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
#include <lightmetrica/surfacegeometry.h>
#include <lightmetrica/surfaceinteraction.h>
#include <lightmetrica/spectrum.h>
#include <functional>
#include <vector>

LM_NAMESPACE_BEGIN

class Scene;
class Random;
struct Photon;
struct Primitive;

///! Utility class for photon density based techniques.
class PhotonMapUtils
{
public:

    struct PathVertex
    {
        int type;
        SurfaceGeometry geom;
        const Primitive* primitive = nullptr;
    };

public:

    LM_DISABLE_CONSTRUCT(PhotonMapUtils);

public:

    ///! Function to parallelize photon tracing
    LM_PUBLIC_API static auto ProcessPhotonTrace(Random* initRng, long long numPhotonTraceSamples, const std::function<void(Random*, std::vector<Photon>&)>& processSampleFunc) -> std::vector<Photon>;

    ///! Function to trace subpath (TODO: refactor it as path sampler)
    LM_PUBLIC_API static auto TraceSubpath(const Scene* scene, Random* rng, int maxNumVertices, TransportDirection transDir, const std::function<bool(int step, const Vec2&, const PathVertex&, const PathVertex&, SPD&)>& processPathVertexFunc) -> void;

};

LM_NAMESPACE_END
