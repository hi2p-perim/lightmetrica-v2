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
#include <lightmetrica/align.h>
#include <lightmetrica/bound.h>
#include <string>
#include <cassert>

LM_NAMESPACE_BEGIN

class TriangleMesh;
class BSDF;
class Emitter;

/*!
	\brief Primitive.

	Primitive is an element of the scene used for managing transformable objects.
	A primitive corresponds to a node in the scene.

    TODO: Redesign sampling related functions.
*/
struct Primitive : public SIMDAlignedType
{

    const char* id = nullptr;
    Mat4 transform;
    Mat3 normalTransform;
    const TriangleMesh* mesh = nullptr;
    const Emitter* emitter = nullptr;
    const BSDF* bsdf = nullptr;

public:

    auto Type() const -> int
    {
        int type = 0;

        if (emitter)
        {
            type |= emitter->Type();
        }
        if (bsdf)
        {
            type |= bsdf->Type();
        }

        return type;
    }

    //auto GetBound() -> Bound
    //{
    //    return mesh ? mesh->GetBound() : Bound();
    //}

    auto SampleDirection(const Vec2& u, Float uComp, int queryType, const SurfaceGeometry& geom, const Vec3& wi, Vec3& wo) const -> void
    {
        assert((queryType & SurfaceInteraction::Emitter) == 0 || (queryType & SurfaceInteraction::BSDF) == 0);
        
        if ((queryType & SurfaceInteraction::Emitter) > 0)
        {
            emitter->SampleDirection(u, uComp, queryType, geom, wi, wo);
            return;
        }
        else if ((queryType & SurfaceInteraction::BSDF) > 0)
        {
            bsdf->SampleDirection(u, uComp, queryType, geom, wi, wo);
            return;
        }

        LM_UNREACHABLE();
    }

    auto EvaluateDirectionPDF(const SurfaceGeometry& geom, int queryType, const Vec3& wi, const Vec3& wo, bool evalDelta) const -> Float
    {
        assert((queryType & SurfaceInteraction::Emitter) == 0 || (queryType & SurfaceInteraction::BSDF) == 0);

        if ((queryType & SurfaceInteraction::Emitter) > 0)
        {
            return emitter->EvaluateDirectionPDF(geom, queryType, wi, wo, evalDelta);
        }
        else if ((queryType & SurfaceInteraction::BSDF) > 0)
        {
            return bsdf->EvaluateDirectionPDF(geom, queryType, wi, wo, evalDelta);
        }

        LM_UNREACHABLE();
        return 0_f;
    }

    auto EvaluateDirection(const SurfaceGeometry& geom, int types, const Vec3& wi, const Vec3& wo, TransportDirection transDir, bool evalDelta) const -> SPD
    {
        assert((types & SurfaceInteraction::Emitter) == 0 || (types & SurfaceInteraction::BSDF) == 0);

        if ((types & SurfaceInteraction::Emitter) > 0)
        {
            return emitter->EvaluateDirection(geom, types, wi, wo, transDir, evalDelta);
        }
        else if ((types & SurfaceInteraction::BSDF) > 0)
        {
            return bsdf->EvaluateDirection(geom, types, wi, wo, transDir, evalDelta);
        }

        LM_UNREACHABLE();
        return SPD();
    }

    auto SamplePosition(const Vec2& u, SurfaceGeometry& geom) const -> void
    {
        assert(emitter);
        emitter->SamplePosition(u, geom);
    }

    auto EvaluatePositionPDF(const SurfaceGeometry& geom, bool evalDelta) const -> Float
    {
        assert(emitter);
        return emitter->EvaluatePositionPDF(geom, evalDelta);
    }

    auto EvaluatePosition(const SurfaceGeometry& geom, bool evalDelta) const -> SPD
    {
        assert(emitter);
        return emitter->EvaluatePosition(geom, evalDelta);
    }

};

LM_NAMESPACE_END
