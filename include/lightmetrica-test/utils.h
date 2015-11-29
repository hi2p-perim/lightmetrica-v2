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

#include <lightmetrica-test/macros.h>
#include <functional>
#include <iostream>
#include <sstream>
#include <regex>

LM_TEST_NAMESPACE_BEGIN

class TestUtils
{
private:

    LM_DISABLE_CONSTRUCT(TestUtils);

public:

    /*!
        Capture standard output.
        Execute the given function and captures all standard outputs.
        Wraps testing::internal::CaptureStdout.
    */
    static auto CaptureStdout(const std::function<void()>& func) -> std::string
    {
        testing::internal::CaptureStdout();
        func();
        return testing::internal::GetCapturedStdout();
    }

    /*!
        Capture standard error.
        Execute the given function and captures all standard outputs.
        Wraps testing::internal::CaptureStderr.
    */
    static auto CaptureStderr(const std::function<void()>& func) -> std::string
    {
        testing::internal::CaptureStderr();
        func();
        return testing::internal::GetCapturedStderr();
    }

    /*!
        Multiline literal with indentation.
    */
    static auto MultiLineLiteral(const std::string& text) -> std::string
    {
        std::string converted;
        const std::regex re(R"x(^ *\| ?(.*)$)x");
        std::stringstream ss(text);
        std::string line;
        while (std::getline(ss, line))
        {
            std::smatch m;
            if (std::regex_match(line, m, re))
            {
                converted += std::string(m[1]) + "\n";
            }
        }
        return converted;
    };

};

LM_TEST_NAMESPACE_END
