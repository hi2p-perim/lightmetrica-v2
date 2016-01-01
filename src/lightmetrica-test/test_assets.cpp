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
#include <lightmetrica/assets.h>
#include <lightmetrica/asset.h>
#include <lightmetrica/property.h>
#include <lightmetrica-test/utils.h>

LM_TEST_NAMESPACE_BEGIN

struct TestAsset : public Asset
{
    LM_INTERFACE_CLASS(TestAsset, Asset);
    LM_INTERFACE_F(Func, int());
};

struct TestAsset1 : public TestAsset
{
    LM_IMPL_CLASS(TestAsset1, TestAsset);
    LM_IMPL_F(Load) = [this](const PropertyNode*, Assets* assets) -> bool { return true; };
    LM_IMPL_F(Func) = [this]() -> int { return 42; };
};

struct TestAsset2 : public TestAsset
{
    LM_IMPL_CLASS(TestAsset2, TestAsset);
    LM_IMPL_F(Load) = [this](const PropertyNode*, Assets* assets) -> bool { return true; };
    LM_IMPL_F(Func) = [this]() -> int { return 43; };
};

LM_COMPONENT_REGISTER_IMPL_DEFAULT(TestAsset1);
LM_COMPONENT_REGISTER_IMPL_DEFAULT(TestAsset2);

TEST(AssetsTest, AssetByIDAndType)
{
    const std::string AssetByIDAndType_Input = TestUtils::MultiLineLiteral(R"x(
    | test_1:
    |   interface: TestAsset
    |   type: TestAsset1
    |
    | test_2:
    |   interface: TestAsset
    |   type: TestAsset2
    )x");

    const auto prop = ComponentFactory::Create<PropertyTree>();
    EXPECT_TRUE(prop->LoadFromString(AssetByIDAndType_Input));

    const auto assets = ComponentFactory::Create<Assets>();
    EXPECT_TRUE(assets->Initialize(prop->Root()));

    {
        const auto* asset = static_cast<const TestAsset*>(assets->AssetByIDAndType("test_1", "TestAsset"));
        ASSERT_NE(nullptr, asset);
        EXPECT_EQ(42, asset->Func());
    }

    {
        const auto* asset = static_cast<const TestAsset*>(assets->AssetByIDAndType("test_2", "TestAsset"));
        ASSERT_NE(nullptr, asset);
        EXPECT_EQ(43, asset->Func());
    }
}

LM_TEST_NAMESPACE_END
