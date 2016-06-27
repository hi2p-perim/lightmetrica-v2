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

#include <lightmetrica/portable.h>
#include <lightmetrica/math.h>
#include <lightmetrica/bsdf.h>
#include <lightmetrica/emitter.h>
#include <lightmetrica/light.h>
#include <lightmetrica/sensor.h>
#include <lightmetrica/align.h>
#include <lightmetrica/bound.h>
#include <string>
#include <cassert>

LM_NAMESPACE_BEGIN

class TriangleMesh;
class BSDF;
class Emitter;
class Light;
class Sensor;

/*!
	\brief Primitive.

	Primitive is an element of the scene used for managing transformable objects.
	A primitive corresponds to a node in the scene.

    TODO: Redesign sampling related functions.

    \ingroup scene
*/
struct Primitive : public SIMDAlignedType
{

    // Primitive ID
    const char* id = nullptr;

    // Primitive index
    size_t index;

    // Transform & normal transform
    Mat4 transform;
    Mat3 normalTransform;

    // Triangle mesh
    const TriangleMesh* mesh = nullptr;

    // Surface interactions
    const BSDF* bsdf       = nullptr;
    const Emitter* emitter = nullptr;
    const Light*  light    = nullptr;
    const Sensor* sensor   = nullptr;

public:

    ///! Get underlying surface interaction types
    auto Type() const -> int
    {
        int type = 0;
        if (bsdf)    { type |= bsdf->Type(); }
        if (emitter) { type |= emitter->Type(); }
        return type;
    }

    auto SampleDirection(const Vec2& u, Float u2, int queryType, const SurfaceGeometry& geom, const Vec3& wi, Vec3& wo) const -> void
    {
        if ((queryType & SurfaceInteractionType::BSDF) > 0)
        {
            bsdf->SampleDirection(u, u2, queryType, geom, wi, wo);
            return;
        }
        if ((queryType & SurfaceInteractionType::Emitter) > 0)
        {
            emitter->SampleDirection(u, u2, queryType, geom, wi, wo);
            return;
        }
        LM_UNREACHABLE();
    };

    auto SamplePositionGivenPreviousPosition(const Vec2& u, const SurfaceGeometry& geomPrev, SurfaceGeometry& geom) const -> void
    {
        assert(emitter != nullptr);
        emitter->SamplePositionGivenPreviousPosition(u, geomPrev, geom);
    };

    auto SamplePositionAndDirection(const Vec2& u, const Vec2& u2, SurfaceGeometry& geom, Vec3& wo) const -> void
    {
        assert(emitter != nullptr);
        emitter->SamplePositionAndDirection(u, u2, geom, wo);
    };

    auto EvaluateDirectionPDF(const SurfaceGeometry& geom, int queryType, const Vec3& wi, const Vec3& wo, bool evalDelta) const -> PDFVal
    {
        if ((queryType & SurfaceInteractionType::Emitter) > 0)
        {
            return emitter->EvaluateDirectionPDF(geom, queryType, wi, wo, evalDelta);
        }
        if ((queryType & SurfaceInteractionType::BSDF) > 0)
        {
            return bsdf->EvaluateDirectionPDF(geom, queryType, wi, wo, evalDelta);
        }
        LM_UNREACHABLE();
        return PDFVal();
    };

    auto EvaluatePositionGivenDirectionPDF(const SurfaceGeometry& geom, const Vec3& wo, bool evalDelta) const -> PDFVal
    {
        assert(emitter != nullptr);
        return emitter->EvaluatePositionGivenDirectionPDF(geom, wo, evalDelta);
    };

    auto EvaluatePositionGivenPreviousPositionPDF(const SurfaceGeometry& geom, const SurfaceGeometry& geomPrev, bool evalDelta) const -> PDFVal
    {
        assert(emitter != nullptr);
        return emitter->EvaluatePositionGivenPreviousPositionPDF(geom, geomPrev, evalDelta);
    };

    auto EvaluateDirection(const SurfaceGeometry& geom, int types, const Vec3& wi, const Vec3& wo, TransportDirection transDir, bool evalDelta) const -> SPD
    {
        if ((types & SurfaceInteractionType::Emitter) > 0)
        {
            return emitter->EvaluateDirection(geom, types, wi, wo, transDir, evalDelta);
        }
        if ((types & SurfaceInteractionType::BSDF) > 0)
        {
            return bsdf->EvaluateDirection(geom, types, wi, wo, transDir, evalDelta);
        }
        LM_UNREACHABLE();
        return SPD();
    };

    auto EvaluatePosition(const SurfaceGeometry& geom, bool evalDelta) const -> SPD
    {
        assert(emitter != nullptr);
        return emitter->EvaluatePosition(geom, evalDelta);
    };

    auto IsDeltaDirection(int type) const -> bool
    {
        if ((type & SurfaceInteractionType::Emitter) > 0)
        {
            return emitter->IsDeltaDirection(type);
        }
        if ((type & SurfaceInteractionType::BSDF) > 0)
        {
            return bsdf->IsDeltaDirection(type);
        }
        LM_UNREACHABLE();
        return false;
    };

    auto IsDeltaPosition(int type) const -> bool
    {
        if ((type & SurfaceInteractionType::Emitter) > 0)
        {
            return emitter->IsDeltaPosition(type);
        }
        if ((type & SurfaceInteractionType::BSDF) > 0)
        {
            return bsdf->IsDeltaPosition(type);
        }
        LM_UNREACHABLE();
        return false;
    };

public:

    auto RasterPosition(const Vec3& wo, const SurfaceGeometry& geom, Vec2& rasterPos) const -> bool
    {
        assert(sensor != nullptr);
        return sensor->RasterPosition(wo, geom, rasterPos);
    }

};

LM_NAMESPACE_END
