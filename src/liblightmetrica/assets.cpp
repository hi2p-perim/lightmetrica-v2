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
#include <lightmetrica/assets.h>
#include <lightmetrica/logger.h>
#include <lightmetrica/property.h>
#include <lightmetrica/asset.h>

LM_NAMESPACE_BEGIN

/*
    The assets are loaded when it queries.
*/
class Assets_ final : public Assets
{
public:

    LM_IMPL_CLASS(Assets_, Assets);

public:

    LM_IMPL_F(Initialize) = [this](const PropertyNode* prop) -> bool
    {
        prop_ = prop;
        return true;
    };

    LM_IMPL_F(AssetByIDAndType) = [this](const std::string& id, const std::string& interfaceType) -> const Asset*
    {
        #pragma region Find the registered asset by id

        const auto it = assetIndexMap_.find(id);
        if (it != assetIndexMap_.end())
        {
            // TODO: Add type check
            return assets_[it->second].get();
        }

        #pragma endregion

        // --------------------------------------------------------------------------------

        #pragma region If not found, try to load asset
        
        // Find property node
        const auto* assetNode = prop_->Child(id);
        if (!assetNode)
        {
            LM_LOG_ERROR("Missing '" + id + "' node");
            return nullptr;
        }

        // Check interface type
        const auto* interfaceNode = assetNode->Child("interface");
        if (!interfaceNode)
        {
            LM_LOG_ERROR("Missing 'interface' node");
            return nullptr;
        }
        if (interfaceType != interfaceNode->As<std::string>())
        {
            LM_LOG_ERROR(boost::str(boost::format("Invalid asset type '%s' expected '%s'") % interfaceNode->As<std::string>() % interfaceType));
            return nullptr;
        }

        // Check the interface inherits `Asset` class
        // TODO

        // Create asset instance
        const auto* typeNode = assetNode->Child("type");
        const auto implType = typeNode->As<std::string>();
        auto asset = ComponentFactory::Create<Asset>(implType.c_str());   // This cannot be const (later we move it)
        if (!asset)
        {
            LM_LOG_ERROR("Failed to create instance: " + implType);
            return nullptr;
        }

        // Check interface type
        //if (asset->Type_().name == interfaceType)
        //{
        //    LM_LOG_ERROR("");
        //    return nullptr;
        //}

        // Load asset
        if (!asset->Load(assetNode->Child("params"), this))
        {
            return nullptr;
        }

        // Register asset
        assets_.push_back(std::move(asset));
        assetIndexMap_[id] = assets_.size() - 1;
        assets_.back()->SetID(id);

        #pragma endregion

        // --------------------------------------------------------------------------------

        return assets_.back().get();
    };

private:

    const PropertyNode* prop_;

private:

    std::vector<Asset::UniquePointerType> assets_;
    std::unordered_map<std::string, size_t> assetIndexMap_;

};

LM_COMPONENT_REGISTER_IMPL(Assets_);

LM_NAMESPACE_END