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
#include <lightmetrica/logger.h>
#include <lightmetrica-test/utils.h>

LM_TEST_NAMESPACE_BEGIN

/*
    Check if the logger macros output
    appropriate message for each log type.
*/
TEST(LoggerTest, LogMessagesWithVariousLevels)
{
    const auto CheckLogOutput = [](const std::string& type, const std::string& message, const std::string& out) -> void
    {
        std::regex re(R"x(^\| ([[:upper:]]+) +[\d]*\.\d{3} \| @[ \d]{4} \| #[ \d]{2} \| (.*)\n)x");
        std::smatch match;
        const bool result = std::regex_match(out, match, re);
        EXPECT_TRUE(result);
        if (result)
        {
            EXPECT_EQ(type, match[1]);
            EXPECT_EQ(message, match[2]);
        }
    };

    CheckLogOutput("ERROR", "Hello", TestUtils::CaptureStdout([]()
    {
        LM_LOG_RUN();
        LM_LOG_ERROR("Hello");
        LM_LOG_STOP();
    }));

    CheckLogOutput("WARN", "Hello", TestUtils::CaptureStdout([]()
    {
        LM_LOG_RUN();
        LM_LOG_WARN("Hello");
        LM_LOG_STOP();
    }));

    CheckLogOutput("INFO", "Hello", TestUtils::CaptureStdout([]()
    {
        LM_LOG_RUN();
        LM_LOG_INFO("Hello");
        LM_LOG_STOP();
    }));

    CheckLogOutput("DEBUG", "Hello", TestUtils::CaptureStdout([]()
    {
        LM_LOG_RUN();
        LM_LOG_DEBUG("Hello");
        LM_LOG_STOP();
    }));
}

/*
    Check if the indentation feature works propertly.
*/
TEST(LoggerTest, Indenter)
{
    const auto ExtractMessage = [](const std::string& out) -> std::string
    {
        std::regex re(R"x(^\| [[:upper:]]+ +[\d]*\.\d{3} \| @[ \d]{4} \| #[ \d]{2} \| (.*))x");
        std::smatch match;
        const bool result = std::regex_match(out, match, re);
        return result ? std::string(match[1]) : "";
    };

    const auto out = TestUtils::CaptureStdout([]()
    {
        LM_LOG_RUN();
     
        LM_LOG_DEBUG("A");
        LM_LOG_INDENTER();
        {
            LM_LOG_DEBUG("B");
            LM_LOG_INDENTER();
            LM_LOG_DEBUG("C");
        }
        LM_LOG_DEBUG("D");
        
        LM_LOG_STOP();
    });

    std::stringstream ss(out);
    const auto NextMessage = [&]() -> std::string
    {
        std::string line;
        std::getline(ss, line, '\n');
        return ExtractMessage(line);
    };

    EXPECT_EQ("A", NextMessage());
    EXPECT_EQ(".... B", NextMessage());
    EXPECT_EQ("........ C", NextMessage());
    EXPECT_EQ(".... D", NextMessage());
}

//
//TEST(LoggerTest, OutputToSignal)
//{
//
//}
//
//TEST(LoggerTest, OutputToFile)
//{
//
//}
//
//TEST(LogerTest, AddLogFromAnotherThread)
//{
//    
//}
//
//TEST(LoggerTest, ImmediateMode)
//{
//    
//}

LM_TEST_NAMESPACE_END
