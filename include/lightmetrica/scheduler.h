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

LM_NAMESPACE_BEGIN

class PropertyNode;
class Scene;
class Film;
class Random;

/*!
    \brief Render scheduler.

    An scheduler for sampling-based rendering technique.
    
    TODO: Remove dependency on std::function to make this class portable.
*/
class Scheduler : public Component
{
public:

    LM_INTERFACE_CLASS(Scheduler, Component);

public:

    Scheduler() = default;
    LM_DISABLE_COPY_AND_MOVE(Scheduler);

public:

    LM_INTERFACE_F(Load, void(const PropertyNode* prop));
    LM_INTERFACE_F(Process, void(const Scene* scene, Film* film, Random* initRng, const std::function<void(const Scene*, Film*, Random*)>& processSampleFunc));

public:

    LM_INTERFACE_CLASS_END(Scheduler);

};

LM_NAMESPACE_END
