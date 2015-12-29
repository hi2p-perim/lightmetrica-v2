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

#include <pch.h>
#include <lightmetrica/detail/stringtemplate.h>
#include <lightmetrica/logger.h>

#if LM_PLATFORM_WINDOWS
#pragma warning(push)
#pragma warning(disable:4267)
#endif
#include <ctemplate/template.h>
#if LM_PLATFORM_WINDOWS
#pragma warning(pop)
#endif

LM_NAMESPACE_BEGIN

auto StringTemplate::Expand(const std::string& input, const std::unordered_map<std::string, std::string>& dict) -> std::string
{
    namespace ct = ctemplate;

    // Convert to ctemplate's dict type
    ct::TemplateDictionary ctdict("dict");
    for (const auto& kv : dict)
    {
        ctdict[kv.first] = kv.second;
    }

    // Expand template
    std::string output;
    auto* tpl = ct::Template::StringToTemplate(input, ct::DO_NOT_STRIP);
    if (!tpl->Expand(&output, &ctdict))
    {
        // TODO: Human-readable error message
        LM_LOG_ERROR("Failed to expand template");
        return "";
    }

    return output;
}

LM_NAMESPACE_END
