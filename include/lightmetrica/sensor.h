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

#include <lightmetrica/emitter.h>

LM_NAMESPACE_BEGIN

class Film;

/*!
    \defgroup sensor Sensor
    \brief Collection of sensors.
    \ingroup asset
*/

/*!
    \brief An interface for Sensor
    \ingroup sensor
*/
class Sensor : public Emitter
{
public:

    LM_INTERFACE_CLASS(Sensor, Emitter, 3);

public:

    Sensor() = default;
    LM_DISABLE_COPY_AND_MOVE(Sensor);

public:

    /*!
        \brief Compute raster position from the direction and the position.

        The function calculates the raster position from the outgoing ray.
        Returns false if calculated raster position is the outside of [0, 1]^2.

        \param wo           Outgoing direction from the point on the emitter.
        \param geom         Surface geometry information around the point on the emitter.
        \param rasterPos    Computed raster position.
    */
    LM_INTERFACE_F(0, RasterPosition, bool(const Vec3& wo, const SurfaceGeometry& geom, Vec2& rasterPos));

    /*!
		\brief Get film.
		Returns the film referenced from the sensor.
		\return Film.
	*/
    LM_INTERFACE_F(1, GetFilm, Film*());

    /*!
        \brief Get projection matrix.
        Returns the projection matrix if available.

        \param zNear Near clip.
        \param zFar  Far clip.
        \return      Projection matrix.
    */
    LM_INTERFACE_F(2, GetProjectionMatrix, Mat4(Float zNear, Float zFar));

};

LM_NAMESPACE_END
