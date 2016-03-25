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

#include <lightmetrica/math.h>
#include <lightmetrica/surfacegeometry.h>
#include <lightmetrica/renderutils.h>
#include <cassert>

LM_NAMESPACE_BEGIN

// TOOD: Make user configurable
enum class PDFMeasure
{
    Null,
    Area,
    SolidAngle,
    ProjectedSolidAngle,
    ProdArea,
    Discrete,
};

struct PDFVal
{

    PDFMeasure measure = PDFMeasure::Null;
    Float v = 0_f;

    PDFVal() {}
    PDFVal(PDFMeasure measure, Float v) : measure(measure), v(v) {}

    //! Convert to area measure.
    auto ConvertToArea(const SurfaceGeometry& geom1, const SurfaceGeometry& geom2) -> PDFVal
    {
        switch (measure)
        {
            case PDFMeasure::SolidAngle:
            {
                PDFVal pdf;
                pdf.measure = PDFMeasure::Area;
                auto p1p2 = geom2.p - geom1.p;
                const auto p1p2L2 = Math::Dot(p1p2, p1p2);
                const auto p1p2L = Math::Sqrt(p1p2L2);
                p1p2 /= p1p2L;
                Float t = 1_f;
                if (!geom2.degenerated) { t *= Math::Abs(Math::Dot(geom2.sn, -p1p2)); }
                pdf.v = v * t / p1p2L2;
                return pdf;
            }

            case PDFMeasure::ProjectedSolidAngle:
            {
                PDFVal pdf;
                pdf.measure = PDFMeasure::Area;
                pdf.v = v * RenderUtils::GeometryTerm(geom1, geom2);
                return pdf;
            }
        }

        LM_UNREACHABLE();
        return PDFVal();
    }

    //! Convert to projected solid angle measure
    auto ConvertToProjSA(const SurfaceGeometry& geom1, const SurfaceGeometry& geom2) -> PDFVal
    {
        switch (measure)
        {
            case PDFMeasure::Area:
            {
                PDFVal pdf;
                pdf.measure = PDFMeasure::Area;
                pdf.v = v / RenderUtils::GeometryTerm(geom1, geom2);
                return pdf;
            }
        }

        LM_UNREACHABLE();
        return PDFVal();
    }

    LM_INLINE auto operator*=(const PDFVal& p) -> PDFVal
    {
        if ((measure == PDFMeasure::ProdArea || p.measure == PDFMeasure::Area) &&
            (measure == PDFMeasure::ProdArea || p.measure == PDFMeasure::Area))
        {
            v *= p.v;
            return *this;
        }

        LM_UNREACHABLE();
        return PDFVal();
    }

};

LM_INLINE auto operator+(const PDFVal& p1, const PDFVal& p2) -> PDFVal
{
    assert(p1.measure == p2.measure);
    return PDFVal(p1.measure, p1.v + p2.v);
}

LM_INLINE auto operator*(const PDFVal& p1, const PDFVal& p2) -> PDFVal
{
    if ((p1.measure == PDFMeasure::ProdArea || p1.measure == PDFMeasure::Area) &&
        (p2.measure == PDFMeasure::ProdArea || p2.measure == PDFMeasure::Area))
    {
        return PDFVal(PDFMeasure::ProdArea, p1.v * p2.v);
    }

    LM_UNREACHABLE();
    return PDFVal();
}

LM_INLINE auto operator*(const PDFVal& p, Float v) -> PDFVal
{
    return PDFVal(p.measure, p.v * v);
}

LM_INLINE auto operator<(const PDFVal& lhs, const PDFVal& rhs) -> bool { assert(lhs.measure == rhs.measure); return lhs.v < rhs.v; }
LM_INLINE auto operator>(const PDFVal& lhs, const PDFVal& rhs) -> bool { return rhs < lhs; }
LM_INLINE auto operator<(const PDFVal& lhs, Float rhs) -> bool { return lhs.v < rhs; }
LM_INLINE auto operator<(Float lhs, const PDFVal& rhs) -> bool { return lhs < rhs.v; }
LM_INLINE auto operator>(const PDFVal& lhs, Float rhs) -> bool { return rhs < lhs; }
LM_INLINE auto operator>(Float lhs, const PDFVal& rhs) -> bool { return rhs < lhs; }


//#pragma region Probability distribution
///*!
//    Probability distribution $P_X$ on the random variable $X$.
//
//    The inherited classes are expected to (implicitly) define
//      - Probability space $(\Omega, F, P)$
//      - Random variable $X: \Omega \to \mathcal{X}$
//      - Probability space with the random variable $X$ : $(\mathcal{X}, \mathcal{A}, \mu)$
//    
//    From the above information (and some additional assumptions) we can derive
//      - CDF $F_X(E) = P(X^{-1}(X)), \forall E \in \mathcal{A}$
//      - PDF $p_\mu(x)$ defined by $P(X \in A) = \inf_{x\in A} p_\mu(x) d\mu(x), \forall A\in F$
//*/
//class PDF : public Component
//{
//public:
//
//    LM_INTERFACE_CLASS(PDF, Component, 0);
//
//public:
//
//    PDF() = default;
//    LM_DISABLE_COPY_AND_MOVE(PDF);
//   
//};
//
//#pragma endregion
//
//// --------------------------------------------------------------------------------
//
//#pragma region Typical distributions
//
//struct SurfaceGeometry;
//
//class AreaPDF : public PDF
//{
//public:
//
//    LM_INTERFACE_CLASS(AreaPDF, PDF, 2);
//
//public:
//
//    AreaPDF() = default;
//    LM_DISABLE_COPY_AND_MOVE(AreaPDF);
//
//public:
//
//    LM_INTERFACE_F(Sample, void(const Vec2& u, SurfaceGeometry& geom));
//    LM_INTERFACE_F(Evaluate, Float(const SurfaceGeometry& geom, bool evalDelta));
//    
//};
//
///*!
//    Distribution for directional sampling on the scene surface.
//      - PDF $p_{\sigma}(\omega_o | \omega_i, \mathbf{x})$
//      - Solid angle measure $\sigma$
//*/
//class DirectionPDF : public PDF
//{
//public:
//
//    LM_INTERFACE_CLASS(DirectionPDF, PDF, 2);
//
//public:
//
//    DirectionPDF() = default;
//    LM_DISABLE_COPY_AND_MOVE(DirectionPDF);
//    
//public:
//
//    LM_INTERFACE_F(Sample, void(const Vec2& u, const SurfaceGeometry& geom, const Vec3& wi, Vec3& wo));
//    LM_INTERFACE_F(Evaluate, Float(const SurfaceGeometry& geom, const Vec3& wi, const Vec3& wo, bool evalDelta));
//
//};
//#pragma endregion

LM_NAMESPACE_END
