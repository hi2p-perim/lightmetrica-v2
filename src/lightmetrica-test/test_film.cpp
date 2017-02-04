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
#include <lightmetrica/film.h>
#include <lightmetrica/property.h>
#include <lightmetrica/logger.h>
#include <lightmetrica-test/utils.h>

LM_TEST_NAMESPACE_BEGIN

#pragma region Fixture

struct FilmTest : public ::testing::TestWithParam<const char*>
{
    virtual auto SetUp() -> void override { Logger::SetVerboseLevel(2); Logger::Run(); }
    virtual auto TearDown() -> void override { Logger::Stop(); }
};

INSTANTIATE_TEST_CASE_P(FilmTypes, FilmTest, ::testing::Values("film::hdr"));

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Tests

TEST_P(FilmTest, Initialize)
{
    const auto prop = ComponentFactory::Create<PropertyTree>();
    ASSERT_TRUE(prop->LoadFromString(TestUtils::MultiLineLiteral(R"x(
    | w: 1000
    | h: 500
    )x")));

    const auto film = ComponentFactory::Create<Film>(GetParam());
    EXPECT_TRUE(film->Load(prop->Root(), nullptr, nullptr));

    EXPECT_EQ(1000, film->Width());
    EXPECT_EQ(500, film->Height());
}

#pragma endregion

LM_TEST_NAMESPACE_END
