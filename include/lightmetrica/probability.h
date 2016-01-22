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
class ProbabilityDist : public Component
{
public:

    LM_INTERFACE_CLASS(ProbabilityDist, Component);

public:

    ProbabilityDist() = default;
    LM_DISABLE_COPY_AND_MOVE(ProbabilityDist);
    
public:

    // Probability space
    // RandomVariableType
    // EvalPDF
    // EvalInverseCDF
    
};

struct SurfaceGeometry;

class ProbabilityDist_Area
{
public:

    LM_INTERFACE_CLASS(ProbabilityDist_Area, Component);

public:

    ProbabilityDist_Area() = default;
    LM_DISABLE_COPY_AND_MOVE(ProbabilityDist_Area);

public:

    LM_INTERFACE_F(EvaluatePDF, Float(const SurfaceGeometry& geom, bool evalDelta));
    LM_INTERFACE_F(Sample, void(const Vec2& u, SurfaceGeometry& geom));
    
};

/*!
    Distribution for directional sampling of hemisphere
      - $\Omega \equiv \mathbb{R}$ : 3D vector
      - $\mathcal{X} \equiv \Omega$ : Solid angle
      - $\mu \equiv \sigma$ : Solid angle measure
*/
class ProbabilityDist_SolidAngle
{
public:

    LM_INTERFACE_CLASS(ProbabilityDist_SolidAngle, Component);

public:

    ProbabilityDist_SolidAngle() = default;
    LM_DISABLE_COPY_AND_MOVE(ProbabilityDist_SolidAngle);
    
public:

    

};


LM_NAMESPACE_END
