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

#include <lightmetrica/accel.h>
#include <lightmetrica/scene.h>
#include <lightmetrica/property.h>
#include <lightmetrica/ray.h>
#include <lightmetrica/intersection.h>
#include <lightmetrica/primitive.h>
#include <lightmetrica/trianglemesh.h>

#include <embree2/rtcore.h>
#include <embree2/rtcore_ray.h>

LM_NAMESPACE_BEGIN

class Accel_Embree : public Accel
{
public:

    LM_IMPL_CLASS(Accel_Embree, Accel);

public:

    LM_IMPL_F(Build) = [this](const Scene& scene) -> bool
    {
        
    };

    LM_IMPL_F(Intersect) = [this](const Scene& scene, const Ray& ray, Intersection& isect) -> bool
    {
        
    };

private:

    

};

LM_COMPONENT_REGISTER_IMPL(Accel_Embree, "embree");

LM_NAMESPACE_END
