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
#include <lightmetrica/surfacegeometry.h>
#include <lightmetrica/probability.h>

LM_NAMESPACE_BEGIN

/*!
    \addtogroup core
    \{
*/

///! Surface interaction type.
namespace SurfaceInteractionType
{
    enum Type
    {
        D = 1 << 0,         //!< Diffuse
        G = 1 << 1,         //!< Glossy
        S = 1 << 2,         //!< Specular
        L = 1 << 3,         //!< Light
        E = 1 << 4,         //!< Sensor
        BSDF = D | G | S,   //!< BSDF flag (D or G or S)
        Emitter = L | E,    //!< Emitter flag (L or E)
        None = 0
    };
};

///! Transport direction type.
enum class TransportDirection
{
    LE,     //!< Light to sensor (L to E)
    EL      //!< Sensor to light (E to L)
};

// --------------------------------------------------------------------------------

///! Base interface for surface interaction types.
class SurfaceInteraction : public Asset
{
public:

    LM_INTERFACE_CLASS(SurfaceInteraction, Asset, 9);

public:

    SurfaceInteraction() = default;
    LM_DISABLE_COPY_AND_MOVE(SurfaceInteraction);

public:

    ///! Get the type of the surface interaction.
    LM_INTERFACE_F(0, Type, int());

public:

    #pragma region Sampling functions

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
        \brief Sample a position on the light given previous position.

        Samples a position on the light from p_A(x|x_prev).
        This PDF is utilized in direct light sampling.

        \param u  Uniform random numbers in [0,1]^2.
        \param u2 Uniform random numbers in [0,1]^2.
        \param geom Surface geometry at the previous position.
        \param geom Surface geometry at the sampled position.
    */
    LM_INTERFACE_F(2, SamplePositionGivenPreviousPosition, void(const Vec2& u, const SurfaceGeometry& geomPrev, SurfaceGeometry& geom));

    /*!
        \brief Sample both position and the direction.
        
        Samples a position and the initial direction of the ray
        from the joint density function p_{A,\sigma^\perp}(x, \omega_o).

        \param u    Uniform random numbers in [0,1]^2.
        \param u2   Uniform random numbers in [0,1]^2.
        \param geom Surface geometry at the sampled position.
        \param wo   Sampled outgoing direction.
    */
    LM_INTERFACE_F(3, SamplePositionAndDirection, void(const Vec2& u, const Vec2& u2, SurfaceGeometry& geom, Vec3& wo));

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
    LM_INTERFACE_F(4, EvaluateDirectionPDF, PDFVal(const SurfaceGeometry& geom, int queryType, const Vec3& wi, const Vec3& wo, bool evalDelta));

    /*!
        \brief Evaluate joint PDF.
        Evaluate the joint PDF p_{A,\sigma^\perp}(x, \omega_o).
    */
    //LM_INTERFACE_F(5, SamplePositionAndDirectionPDF, Float(const Vec2& u, const Vec2& u2, SurfaceGeometry& geom, Vec3& wo));

    /*!
        \brief Evaluate positional PDF.

        The joint PDF $p_{A,\sigma^\perp}(x, \omega_o)$ can be decomposed into three cases

          - $p_A(x) p_{\sigma^\perp}(\omega_o | x)$
            e.g., area light

          - $p_{\sigma^\perp}(\omega_o) p_A(x | \omega_o)$
            e.g., environment light, directional light

          - $p_A(x) p_{\sigma^\perp}(\omega_o)$
            e.g., point light, pinhole camera

        This function evaluate each terms according to the combination of parameters.
        The options are:

          - $p_A(x)$
            Some PDFs has an independent distribution,
            for such a case, this density is automatically evaluated.
            For this types of densities, `direct` flag has no effect if enabled or disabled.
          
          - $p_A(x | \omega_o)$
            Evaluate PDF given the direction of the ray.
            Disable `direct` flag to use this option.
          
          - $p_A(x | x_prev)$
            Evaluate PDF for direct light sampling.
            Enable `direct` flag to use this option.

        If `evalDelta` flag is enabled, the PDF try to evaluate
        underlying delta function of the same measure as density as it is, otherwise ignored.

        \param geom       Surface geometry.
        \param direct     Select a PDF for direct light sampling if enabled.
        \param evalDelta  Evaluates underlying delta function if enabled.
        \return           Evaluated PDF.
    */
    LM_INTERFACE_F(5, EvaluatePositionGivenDirectionPDF,        PDFVal(const SurfaceGeometry& geom, const Vec3& wo, bool evalDelta));
    LM_INTERFACE_F(6, EvaluatePositionGivenPreviousPositionPDF, PDFVal(const SurfaceGeometry& geom, const SurfaceGeometry& geomPrev, bool evalDelta));

    enum class PositionPDFTypes
    {
        Independent,            // p_A(x)
        GivenDirection,         // p_A(x | \omega_o)
        GivenPreviousPosition,  // p_A(x | x_prev)
    };

    /*!
        \brief Helper function for evaluating positional PDFs.
        \param type      PDF type.     
        \param geom      Surface geometry.
        \param geomPrev  Surface geometry for previous point (optional).
        \param wo        Outgoing ray direction incident to the position.
        \param evalDelta Evaluates underlying delta function if enabled.
    */
    auto EvaluatePositionPDF(PositionPDFTypes type, const SurfaceGeometry& geom, const SurfaceGeometry& geomPrev, const Vec3& wo, bool evalDelta) -> Float
    {
        // TODO
        return 0_f;
    }

    #pragma endregion

public:

    #pragma region Evaluation of position and directional components

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

        In order to handle the asymmetric case of BSDF [Veach 1998],
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
    LM_INTERFACE_F(7, EvaluateDirection, SPD(const SurfaceGeometry& geom, int types, const Vec3& wi, const Vec3& wo, TransportDirection transDir, bool evalDelta));

    /*!
        \brief Evaluate the positional component of the emitted quantity.
        \param geom       Surface geometry.
        \param evalDelta  Evaluates underlying delta function if enabled.
        \return           Positional component of the emitted quantity.
    */
    LM_INTERFACE_F(8, EvaluatePosition, SPD(const SurfaceGeometry& geom, bool evalDelta));

    #pragma endregion

};

//! \}

LM_NAMESPACE_END
