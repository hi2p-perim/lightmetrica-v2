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
#include <lightmetrica/detail/propertyutils.h>
#include <lightmetrica/property.h>
#include <lightmetrica/logger.h>

LM_NAMESPACE_BEGIN

auto PropertyUtils::PrintPrettyError(const PropertyNode* node) -> void
{
    int line = node->Line();
    const auto path = node->Tree()->Path();
    const auto filename = boost::filesystem::path(path).filename().string();
    LM_LOG_ERROR("Error around line " + std::to_string(line) + " @ " + filename);
    std::ifstream fs(path);
    for (int i = 0; i <= line + 2; i++)
    {
        std::string t;
        std::getline(fs, t, '\n');
        if (line - 2 <= i && i <= line + 2)
        {
            LM_LOG_ERROR(boost::str(boost::format("% 4d%c| %s") % i % (i == line ? '*' : ' ') % t));
        }
    }
}

LM_NAMESPACE_END
