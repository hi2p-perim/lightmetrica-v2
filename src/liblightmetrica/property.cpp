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
#include <lightmetrica/property.h>
#include <lightmetrica/logger.h>
#include <yaml-cpp/yaml.h>

LM_NAMESPACE_BEGIN

class PropertyTree_;

class PropertyNode_ final : public PropertyNode
{
    friend class PropertyTree_;

public:

    LM_IMPL_CLASS(PropertyNode_, PropertyNode);

public:

    LM_IMPL_F(Type)   = [this]() -> PropertyNodeType { return type_; };
    LM_IMPL_F(Line)   = [this]() -> int { return line_; };
    LM_IMPL_F(Column) = [this]() -> int { return column_; };
    LM_IMPL_F(Scalar) = [this]() -> std::string { return scalar_; };
    LM_IMPL_F(Key)    = [this]() -> std::string { return key_; };
    LM_IMPL_F(Size)   = [this]() -> int { return (int)(sequence_.size()); };
    LM_IMPL_F(Child)  = [this](const std::string& key) -> const PropertyNode* { const auto it = map_.find(key); return it != map_.end() ? it->second : nullptr; };
    LM_IMPL_F(At)     = [this](int index) -> const PropertyNode* { return sequence_.at(index); };
    LM_IMPL_F(Parent) = [this]() -> const PropertyNode* { return parent_; };

private:

    // Type of the node
    PropertyNodeType type_;

    // Line, column
    int line_;
    int column_;

    // For map node type
    std::string key_;
    std::unordered_map<std::string, const PropertyNode_*> map_;

    // For sequence node type
    std::vector<const PropertyNode_*> sequence_;

    // For scalar type
    std::string scalar_;

    // Parent node (nullptr for root node)
    const PropertyNode_* parent_ = nullptr;

};

LM_COMPONENT_REGISTER_IMPL(PropertyNode_);

// --------------------------------------------------------------------------------

class PropertyTree_ final : public PropertyTree
{
public:

    LM_IMPL_CLASS(PropertyTree_, PropertyTree);

public:

    LM_IMPL_F(LoadFromString) = [this](const std::string& input) -> bool
    {
        #pragma region Traverse YAML nodes and convert to the our node type

        const std::function<PropertyNode_*(const YAML::Node&)> Traverse = [&](const YAML::Node& yamlNode) -> PropertyNode_*
        {
            // Create our node
            nodes_.push_back(ComponentFactory::Create<PropertyNode>());
            auto* node_internal = static_cast<PropertyNode_*>(nodes_.back().get());
            node_internal->line_ = yamlNode.Mark().line;
            node_internal->column_ = yamlNode.Mark().column;

            switch (yamlNode.Type())
            {
                case YAML::NodeType::Null:
                {
                    node_internal->type_ = PropertyNodeType::Null;
                    break;
                }
                
                case YAML::NodeType::Scalar:
                {
                    node_internal->type_ = PropertyNodeType::Scalar;
                    node_internal->scalar_ = yamlNode.Scalar();
                    break;
                }

                case YAML::NodeType::Sequence:
                {
                    node_internal->type_ = PropertyNodeType::Sequence;
                    for (size_t i = 0; i < yamlNode.size(); i++)
                    {
                        node_internal->sequence_.push_back(Traverse(yamlNode[i]));
                    }
                    break;
                }

                case YAML::NodeType::Map:
                {
                    node_internal->type_ = PropertyNodeType::Map;
                    for (const auto& p : yamlNode)
                    {
                        const auto key = p.first.as<std::string>();
                        auto* childNode = Traverse(p.second);
                        childNode->key_ = key;
                        childNode->parent_ = node_internal;
                        node_internal->map_[key] = childNode;
                    }
                    break;
                }

                case YAML::NodeType::Undefined:
                {
                    LM_UNREACHABLE();
                    break;
                }
            }

            return node_internal;
        };

        try
        {
            const auto root = YAML::Load(input.c_str());
            root_ = Traverse(root);
        }
        catch (const YAML::Exception& e)
        {
            LM_LOG_ERROR("YAML exception: " + std::string(e.what()));
            return false;
        }

        #pragma endregion

        return true;
    };

    LM_IMPL_F(Root) = [this]() -> const PropertyNode*
    {
        return root_;
    };

private:

    const PropertyNode* root_;
    std::vector<PropertyNode::UniquePtr> nodes_;

};

LM_COMPONENT_REGISTER_IMPL(PropertyTree_);

LM_NAMESPACE_END
