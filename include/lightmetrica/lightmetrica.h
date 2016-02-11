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

#include <lightmetrica/accel.h>
#include <lightmetrica/align.h>
#include <lightmetrica/asset.h>
#include <lightmetrica/assets.h>
#include <lightmetrica/bound.h>
#include <lightmetrica/bsdf.h>
#include <lightmetrica/bsdfutils.h>
#include <lightmetrica/component.h>
#include <lightmetrica/configurable.h>
#include <lightmetrica/debug.h>
#include <lightmetrica/dist.h>
#include <lightmetrica/emitter.h>
#include <lightmetrica/enum.h>
#include <lightmetrica/exception.h>
#include <lightmetrica/film.h>
#include <lightmetrica/fp.h>
#include <lightmetrica/generalizedbsdf.h>
#include <lightmetrica/intersection.h>
#include <lightmetrica/intersectionutils.h>
#include <lightmetrica/light.h>
#include <lightmetrica/lightmetrica.h>
#include <lightmetrica/logger.h>
#include <lightmetrica/macros.h>
#include <lightmetrica/math.h>
#include <lightmetrica/metacounter.h>
#include <lightmetrica/portable.h>
#include <lightmetrica/primitive.h>
#include <lightmetrica/property.h>
#include <lightmetrica/random.h>
#include <lightmetrica/ray.h>
#include <lightmetrica/reflection.h>
#include <lightmetrica/renderer.h>
#include <lightmetrica/renderutils.h>
#include <lightmetrica/sampler.h>
#include <lightmetrica/scene.h>
#include <lightmetrica/scheduler.h>
#include <lightmetrica/sensor.h>
#include <lightmetrica/spectrum.h>
#include <lightmetrica/static.h>
#include <lightmetrica/statictest.h>
#include <lightmetrica/surfacegeometry.h>
#include <lightmetrica/surfaceinteraction.h>
#include <lightmetrica/texture.h>
#include <lightmetrica/trianglemesh.h>
#include <lightmetrica/triangleutils.h>

