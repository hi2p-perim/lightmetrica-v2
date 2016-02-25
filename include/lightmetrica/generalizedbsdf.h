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

#include <lightmetrica/asset.h>
#include <lightmetrica/math.h>
#include <lightmetrica/spectrum.h>
#include <lightmetrica/surfaceinteraction.h>

LM_NAMESPACE_BEGIN

struct SurfaceGeometry;
class DirectionSampler;

/*!
    \brief An interface for generalized BSDF.
    \ingroup asset
*/
class GeneralizedBSDF : public Asset
{
public:

    LM_INTERFACE_CLASS(GeneralizedBSDF, Asset, 4);

public:

    GeneralizedBSDF() = default;
    LM_DISABLE_COPY_AND_MOVE(GeneralizedBSDF);

public:

    /*!
        \brief Generalized BSDF type.
        \return Type.
    */
    LM_INTERFACE_F(0, Type, int());

    /*!
        \brief Sample outgoing vector.

        Given the input direction `wi` originated from
        the point `geom.p` on the surface. The function samples
        outgoing vector `wo` from the distribution in
        the solid angle measure according to the implementation.
        
        Mathematically the function sample the direction from
        the distribution $p_{\sigma^\perp}(\omega_o | \omega_i, \mathbf{x})$.
        where $\sigma^\perp$ is the projected solid angle measure,
        $\omega_i$ is the incident direction, and $\mathbf{x}$ is
        the surface position on the scene.

        \param u            Uniform random number in [0,1]^2.
        \param uComp        Uniform random number for component selection etc. in [0,1].
        \param queryType    SurfaceInteraction type for the sample query.
        \param geom         Surface geometry information of the surface point.
        \param wi           Incident direction.
        \param wo           Sampled outgoing direction.
    */
    LM_INTERFACE_F(1, SampleDirection, void(const Vec2& u, Float u2, int queryType, const SurfaceGeometry& geom, const Vec3& wi, Vec3& wo));

    /*!
        \brief Evaluate PDF with the direction.

        Evaluate the distribution used in SampleDirection.
        That is, $p_{\sigma^\perp}(\omega_o | \omega_i, \mathbf{x})$.

        Some PDF contains delta component bound to some measures
        (e.g., $\delta_{\sigma^\perp}(\omega_o)$ for specular reflection).
        we can ignore the delta component of the distribution by setting
        `evalDelta` to false. 

        \param queryType    SurfaceInteraction type for the sample query.
        \param geom         Surface geometry information of the surface point.
        \param wi           Incident direction.
        \param wo           Outgoing direction.
        \param evalDelta    `true` if we do not want to ignore the delta function
                            in the distribution, otherwise `false`.
    */
    LM_INTERFACE_F(2, EvaluateDirectionPDF, Float(const SurfaceGeometry& geom, int queryType, const Vec3& wi, const Vec3& wo, bool evalDelta));
    
    /*!
        \brief Evaluate generalized BSDF.

        The evaluation of the function differs according to
        the underlying generalized BSDF types.
        If the type is `BSDF`, the function evaluates the BSDF $f_s$.
        If the type is `L`, the function evaluates $L_e$.
        If the type is `W`, the function evaluates $W_e$.
        
        Also some BSDFs might contain delta functions.
        Again we can ignore the delta component of the distribution
        by setting `evalDelta` to false. 

        In order to handle the asymetric case of BSDF [Veach 1998],
        we also need to pass the direction of light transport `transDir`.

        \param types        SurfaceInteraction type for the evaluation query.
        \param geom         Surface geometry information of the surface point.
        \param wi           Incident direction.
        \param wo           Outgoing direction.
        \param transDir     Transport direction of the light transport.
        \param evalDelta    `true` if we do not want to ignore the delta function
                            in the distribution, otherwise `false`.
        \return             Evaluated contribution.
    */
    LM_INTERFACE_F(3, EvaluateDirection, SPD(const SurfaceGeometry& geom, int types, const Vec3& wi, const Vec3& wo, TransportDirection transDir, bool evalDelta));

};

LM_NAMESPACE_END
