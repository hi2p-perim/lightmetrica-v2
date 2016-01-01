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

/*!
    \brief Configurable components.

    Component with initialized with user defined types and properties.
    The configurable component has same format in the scene configuration file:
    
      component_name:
        type: component_type
        params:
          param_1: ...
          param_2: ...
          ...
*/
class Configurable : public Component
{
public:

    LM_INTERFACE_CLASS(Configurable, Component);
    
public:

    Configurable() = default;
    LM_DISABLE_COPY_AND_MOVE(Configurable);

public:

    LM_INTERFACE_F(Initialize, bool(const PropertyNode*));

};

LM_NAMESPACE_END
