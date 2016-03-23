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
#include <lightmetrica/math.h>

LM_NAMESPACE_BEGIN

#pragma region Probability distribution

/*!
    Probability distribution $P_X$ on the random variable $X$.

    The inherited classes are expected to (implicitly) define
      - Probability space $(\Omega, F, P)$
      - Random variable $X: \Omega \to \mathcal{X}$
      - Probability space with the random variable $X$ : $(\mathcal{X}, \mathcal{A}, \mu)$
    
    From the above information (and some additional assumptions) we can derive
      - CDF $F_X(E) = P(X^{-1}(X)), \forall E \in \mathcal{A}$
      - PDF $p_\mu(x)$ defined by $P(X \in A) = \inf_{x\in A} p_\mu(x) d\mu(x), \forall A\in F$
*/
class PDF : public Component
{
public:

    LM_INTERFACE_CLASS(PDF, Component, 0);

public:

    PDF() = default;
    LM_DISABLE_COPY_AND_MOVE(PDF);
   
};

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Typical distributions

struct SurfaceGeometry;

class AreaPDF : public PDF
{
public:

    LM_INTERFACE_CLASS(AreaPDF, PDF, 2);

public:

    AreaPDF() = default;
    LM_DISABLE_COPY_AND_MOVE(AreaPDF);

public:

    LM_INTERFACE_F(Sample, void(const Vec2& u, SurfaceGeometry& geom));
    LM_INTERFACE_F(Evaluate, Float(const SurfaceGeometry& geom, bool evalDelta));
    
};

/*!
    Distribution for directional sampling on the scene surface.
      - PDF $p_{\sigma}(\omega_o | \omega_i, \mathbf{x})$
      - Solid angle measure $\sigma$
*/
class DirectionPDF : public PDF
{
public:

    LM_INTERFACE_CLASS(DirectionPDF, PDF, 2);

public:

    DirectionPDF() = default;
    LM_DISABLE_COPY_AND_MOVE(DirectionPDF);
    
public:

    LM_INTERFACE_F(Sample, void(const Vec2& u, const SurfaceGeometry& geom, const Vec3& wi, Vec3& wo));
    LM_INTERFACE_F(Evaluate, Float(const SurfaceGeometry& geom, const Vec3& wi, const Vec3& wo, bool evalDelta));

};

#pragma endregion

LM_NAMESPACE_END
