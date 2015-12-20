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

    LM_IMPL_F(Initialize) = [this](const PropertyNode*) -> bool
    {
        return false;
    };

private:
    
    auto Initialize(const PropertyNode* prop) -> bool
    {
        // Create instances
        assets_ = ComponentFactory::Create<Assets>();

        // --------------------------------------------------------------------------------

        #pragma region Load primitives
        
        {
            // `scene` node
            // TODO: Add error check (with detailed and human-readable error message)
            const auto* scenePropNode = prop->Child("scene");

            // Traverse scene nodes and create primitives
            const auto Traverse = [this](const PropertyNode* propNode) -> void
            {
                // ID
                std::string id;
                {
                    const auto* idNode = propNode->Child("id");
                    if (idNode)
                    {
                        id = idNode->As<std::string>();
                    }
                }

                // Transform
                

                // Child nodes
                const auto* childNode = propNode->Child("child");
                if (childNode)
                {
                    
                }
            };

            const auto* rootPropNode = scenePropNode->Child("nodes");
            Traverse(rootPropNode);
        }

        #pragma endregion

        // --------------------------------------------------------------------------------

        return true;
    };

private:

    Assets::UniquePointerType assets_{nullptr, [](Component*){}};     // Asset library
    //std::vector<std::unique_ptr<Primitive>> primitives_;            // Primitives
    //std::unordered_map<std::string, Primitive*> primitiveIDMap_;    // Mapping from ID to primitive pointer
    //std::unique_ptr<Accel> accel_;                                  // Acceleration structure

};

LM_COMPONENT_REGISTER_IMPL(Scene_);

LM_NAMESPACE_END
