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
#include <lightmetrica/scene.h>
#include <lightmetrica/math.h>
#include <lightmetrica/property.h>
#include <lightmetrica/assets.h>
#include <lightmetrica/primitive.h>
#include <lightmetrica/accel.h>
#include <glm/glm.hpp>

LM_NAMESPACE_BEGIN

class Scene_ final : public Scene
{
public:

    LM_IMPL_CLASS(Scene_, Scene);

public:

    LM_IMPL_F(Initialize) = [this](const PropertyNode* root) -> bool
    {
        // Create instances
        assets_ = ComponentFactory::Create<Assets>();
        
        // Scene configuration file must begin with `lightmetrica_scene` node
        const auto* prop = root->Child("lightmetrica_scene");
        if (!prop)
        {
            // TODO: Improve error messages
            LM_LOG_ERROR("Missing 'lightmetrica_scene' node");
            return false;
        }

        // --------------------------------------------------------------------------------

        #pragma region Load primitives
        
        {
            // `scene` node
            // TODO: Add error check (with detailed and human-readable error message)
            const auto* scenePropNode = prop->Child("scene");
            if (!scenePropNode)
            {
                LM_LOG_ERROR("Missing 'scene' node");
                return false;
            }

            // Traverse scene nodes and create primitives
            const std::function<void(const PropertyNode*)> Traverse = [&](const PropertyNode* propNode) -> void
            {
                // Create primitive
                std::unique_ptr<Primitive> primitive(new Primitive);

                // ID
                {
                    const auto* idNode = propNode->Child("id");
                    if (idNode)
                    {
                        primitive->id = idNode->As<std::string>();
                    }
                }

                // Transform
                {
                    Mat4 transform;
                    primitive->transform = transform;
                }

                // Add primitive
                if (!primitive->id.empty())
                {
                    primitiveIDMap_[primitive->id] = primitive.get();
                }
                primitives_.push_back(std::move(primitive));
                
                // --------------------------------------------------------------------------------

                // Child nodes
                const auto* childNode = propNode->Child("child");
                if (childNode)
                {
                    for (int i = 0; i < childNode->Size(); i++)
                    {
                        Traverse(childNode->At(i));
                    }
                }
            };

            const auto* rootPropNode = scenePropNode->Child("nodes");
            for (int i = 0; i < rootPropNode->Size(); i++)
            {
                Traverse(rootPropNode->At(i));
            }
        }

        #pragma endregion

        // --------------------------------------------------------------------------------

        return true;
    };

    LM_IMPL_F(PrimitiveByID) = [this](const std::string& id) -> const Primitive*
    {
        const auto it = primitiveIDMap_.find(id);
        return it != primitiveIDMap_.end() ? it->second : nullptr;
    };

private:

    Assets::UniquePointerType assets_{nullptr, [](Component*){}};     // Asset library
    std::vector<std::unique_ptr<Primitive>> primitives_;              // Primitives
    std::unordered_map<std::string, Primitive*> primitiveIDMap_;      // Mapping from ID to primitive pointer
    //std::unique_ptr<Accel> accel_;                                  // Acceleration structure

};

LM_COMPONENT_REGISTER_IMPL(Scene_);

LM_NAMESPACE_END
