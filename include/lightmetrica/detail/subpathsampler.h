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
struct Primitive;

///! Utility class for sampling subpaths.
class SubpathSampler
{
public:

    enum class SampleUsage
    {
        EmitterSelection,
        Position,
        Direction,
        ComponentSelection,
    };

    struct PathVertex
    {
        int type;
        SurfaceGeometry geom;
        const Primitive* primitive = nullptr;
    };

public:

    LM_DISABLE_CONSTRUCT(SubpathSampler);

public:

    /*!
        Sampler function type for determining the next sample
        this function is called when the sampler requiest a sample for the primitive and 
        the specified usage.
    */
    using SamplerFunc = std::function<Float(const Primitive* primitive, SubpathSampler::SampleUsage usage, int index)>;

    /*!
        Callback function type for processing path vertices.

        \param step       Index of the path vertices currently processing (1-indexed).
        \param rasterPos  Raster position (if available).
        \param pv         Previous path vertex.
        \param v          Current path vertex.
        \param throughput Throughput of the path.
        \retval true      Specify to continue the path.
        \retval false     Specify not to continue the path.
    */
    using ProcessPathVertexFunc = std::function<bool(int numVertices, const Vec2& rasterPos, const PathVertex& pv, const PathVertex& v, SPD& throughput)>;

public:

    /*!
        Function to trace subpath.

        \param scene                 Scene.
        \param rng                   Random number generator.
        \param maxNumVertices        Maximum number of vertices in the subpath.
        \param transDir              Transport direction of the subpath.
        \param processPathVertexFunc Callback function to process vertices.
    */
    LM_PUBLIC_API static auto TraceSubpath(const Scene* scene, Random* rng, int maxNumVertices, TransportDirection transDir, const ProcessPathVertexFunc& processPathVertexFunc) -> void;

    /*!
        Function to trace eye subpath with fixed raster position.

        \param scene                 Scene.
        \param rng                   Random number generator.
        \param maxNumVertices        Maximum number of vertices in the subpath.
        \param transDir              Transport direction of the subpath.
        \param rasterPos             Fixed raster position.
        \param processPathVertexFunc Callback function to process vertices.
    */
    LM_PUBLIC_API static auto TraceEyeSubpathFixedRasterPos(const Scene* scene, Random* rng, int maxNumVertices, TransportDirection transDir, const Vec2& rasterPos, const ProcessPathVertexFunc& processPathVertexFunc) -> void;

    /*!
        Function to trance subpath from current endpoint.

        \param scene                 Scene.
        \param rng                   Random number generator.
        \param pv                    Last vertex. Specify nullptr if not available.
        \param ppv                   Second last vertex. Specify nullptr if not available.
        \param nv                    Current number of vertices.
        \param maxNumVertices        Maximum number of vertices in the subpath.
        \param transDir              Transport direction of the subpath.
        \param processPathVertexFunc Callback function to process vertices.
    */
    LM_PUBLIC_API static auto TraceSubpathFromEndpoint(const Scene* scene, Random* rng, const PathVertex* pv, const PathVertex* ppv, int nv, int maxNumVertices, TransportDirection transDir, const ProcessPathVertexFunc& processPathVertexFunc) -> void;

    /*!
        Function to trance subpath from current endpoint with given sampler.

        \param scene                 Scene.
        \param pv                    Last vertex. Specify nullptr if not available.
        \param ppv                   Second last vertex. Specify nullptr if not available.
        \param nv                    Current number of vertices.
        \param maxNumVertices        Maximum number of vertices in the subpath.
        \param transDir              Transport direction of the subpath.
        \param sampleNext            Sampler function.
        \param processPathVertexFunc Callback function to process vertices.
    */
    LM_PUBLIC_API static auto TraceSubpathFromEndpointWithSampler(const Scene* scene, const PathVertex* pv, const PathVertex* ppv, int nv, int maxNumVertices, TransportDirection transDir, const SamplerFunc& sampleNext, const ProcessPathVertexFunc& processPathVertexFunc) -> void;

};

LM_NAMESPACE_END
