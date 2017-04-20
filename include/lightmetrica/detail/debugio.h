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
#include <string>
#include <sstream>
#include <functional>

LM_NAMESPACE_BEGIN

class DebugIO
{
public:

    LM_DISABLE_CONSTRUCT(DebugIO);

public:

    static auto Run() -> void;
    static auto Stop() -> void;
    static auto Input() ->std::string;
    static auto Output(const std::string& tag, const std::string& out) -> void;
    static auto Connected() -> bool;
    static auto Wait() -> bool;

public:

    static auto BreakPoint(const std::string& name, const std::function<std::string()>& serializeFunc) -> void
    {
        LM_LOG_DEBUG(name);
        Wait();
        const auto s = serializeFunc();
        Output(name, s);
        Wait();
    }

    static auto BreakPoint(const std::string& name, const BasicComponent* o) -> void
    {
        if (!o->Serialize.Implemented())
        {
            LM_LOG_ERROR("Unserializable component. Skipping.");
            return;
        }
        LM_TBA_RUNTIME();
        //BreakPoint(name, [&]()
        //{
        //    return o->Serialize();
        //});
    }

};

LM_NAMESPACE_END
