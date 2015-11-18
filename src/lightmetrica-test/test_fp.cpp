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

#include <pch_test.h>
#include <lightmetrica/exception.h>
#include <lightmetrica/fp.h>
#include <lightmetrica-test/utils.h>

LM_TEST_NAMESPACE_BEGIN

TEST(FPTest, CatchSupportedExceptions)
{
    const auto GetDescription = [](const std::string& out) -> std::string
    {
        std::regex re(R"x(Description +: ([\w_]+))x");
        std::smatch m;
        return std::regex_search(out, m, re) ? m[1] : std::string("");
    };

    const auto Trial = [&](const std::string& desc, const std::function<void()>& func) -> void
    {
        SEHUtils::EnableStructuralException();
        FPUtils::EnableFPControl();

        bool exception = false;
        try
        {
            func();
        }
        catch (const std::runtime_error& e)
        {
            exception = true;
            EXPECT_EQ(desc, std::string(e.what()));
        }
        EXPECT_TRUE(exception);

        FPUtils::DisableFPControl();
        SEHUtils::DisableStructuralException();
    };

    // --------------------------------------------------------------------------------

    Trial("FLT_INVALID_OPERATION", [&]()
    {
        const double t = std::numeric_limits<double>::infinity() * 0;
        LM_UNUSED(t);
    });

    Trial("FLT_INVALID_OPERATION", [&]()
    {
        double z = 0;
        const double t = 0 / z;
        LM_UNUSED(t);
    });

    Trial("FLT_INVALID_OPERATION", [&]()
    {
        std::sqrt(-1);
    });

    Trial("FLT_INVALID_OPERATION", [&]()
    {
        const double t = 1.0 * std::numeric_limits<double>::signaling_NaN();
        LM_UNUSED(t);
    });

    Trial("FLT_DIVIDE_BY_ZERO", [&]()
    {
        double z = 0;
        const double t = 1.0 / z;
        LM_UNUSED(t);
    });
}

TEST(FPTest, UnsupportedException)
{

}

LM_TEST_NAMESPACE_END
