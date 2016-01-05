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
#include <lightmetrica/trianglemesh.h>
#include <lightmetrica/light.h>
#include <lightmetrica/sensor.h>
#include <lightmetrica/bsdf.h>
#include <lightmetrica/ray.h>
#include <lightmetrica/intersection.h>
#include <lightmetrica/detail/propertyutils.h>

LM_NAMESPACE_BEGIN

class Scene_ final : public Scene
{
public:

    LM_IMPL_CLASS(Scene_, Scene);

public:

    LM_IMPL_F(Initialize) = [this](const PropertyNode* sceneNode, Assets* assets, Accel* accel) -> bool
    {
        #pragma region Load primitives
        
        {
            LM_LOG_INFO("Loading primitives");
            LM_LOG_INDENTER();

            #pragma region Traverse scene nodes and create primitives

            const std::function<bool(const PropertyNode*, const Mat4&)> Traverse = [&](const PropertyNode* propNode, const Mat4& parentTransform) -> bool
            {
                #pragma region Create primitive

                std::unique_ptr<Primitive> primitive(new Primitive);

                LM_LOG_INFO("Traversing node");
                LM_LOG_INDENTER();

                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Parse ID
                
                {
                    const auto* idNode = propNode->Child("id");
                    if (idNode)
                    {
                        primitive->id = idNode->As<const char*>();
                        LM_LOG_INFO("ID: '" + std::string(primitive->id) + "'");
                    }
                }

                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Parse transform

                Mat4 transform;

                {
                    LM_LOG_INFO("Parsing transform");
                    LM_LOG_INDENTER();

                    const auto* transformNode = propNode->Child("transform");
                    if (!transformNode)
                    {
                        // Missing `transform` node, identity matrix is assumed
                        primitive->transform = Mat4::Identity();
                    }
                    else
                    {
                        // Parse transform from the node
                        // There are several ways to specify a transformation
                        const auto ParseTransform = [](const PropertyNode* transformNode, Mat4& transform) -> bool
                        {
                            // `matrix` node
                            const auto* matrixNode = transformNode->Child("matrix");
                            if (matrixNode)
                            {
                                // Parse 4x4 matrix
                                transform = matrixNode->As<Mat4>();
                                return true;
                            }

                            // `translate`, `rotate`, or `scale` node
                            const auto* translateNode = transformNode->Child("translate");
                            const auto* rotateNode    = transformNode->Child("rotate");
                            const auto* scaleNode     = transformNode->Child("scale");
                            if (translateNode || rotateNode || scaleNode)
                            {
                                transform = Mat4::Identity();

                                // Parse 'translate' node
                                if (translateNode)
                                {
                                    transform *= Math::Translate(translateNode->As<Vec3>());
                                }

                                // Parse 'rotate' node
                                if (rotateNode)
                                {
                                    const auto angleNode = rotateNode->Child("axis");
                                    const auto axisNode  = rotateNode->Child("angle");
                                    if (!angleNode || !axisNode)
                                    {
                                        LM_LOG_ERROR("Missing 'angle' or 'axis' node");
                                        PropertyUtils::PrintPrettyError(rotateNode);
                                        return false;
                                    }

                                    transform *= Math::Rotate(Math::Radians(axisNode->As<Float>()), angleNode->As<Vec3>());
                                }

                                // Parse 'scale' node
                                if (scaleNode)
                                {
                                    transform *= Math::Scale(scaleNode->As<Vec3>());
                                }

                                return true;
                            }

                            transform = Mat4::Identity();
                            return true;
                        };
                        
                        if (!ParseTransform(transformNode, transform))
                        {
                            return false;
                        }

                        transform = parentTransform * transform;
                        primitive->transform = transform;
                    }
                }

                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Emitter

                const auto* L = propNode->Child("light");
                const auto* E = propNode->Child("sensor");
                if (L && E)
                {
                    LM_LOG_ERROR("'light' and 'sensor' node cannot be used in the same time");
                    PropertyUtils::PrintPrettyError(L);
                    PropertyUtils::PrintPrettyError(E);
                    return false;
                }
                else if (L || E)
                {
                    if (L)
                    {
                        primitive->emitter = static_cast<const Emitter*>(assets->AssetByID<Light>(L->As<std::string>()));
                    }
                    else if (E)
                    {
                        primitive->emitter = static_cast<const Emitter*>(assets->AssetByID<Sensor>(E->As<std::string>()));
                    }
                    if (!primitive->emitter)
                    {
                        LM_LOG_ERROR("Failed to create emitter");
                        PropertyUtils::PrintPrettyError(L ? L : E);
                        return false;
                    }
                }

                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Triangle mesh

                const auto* meshNode = propNode->Child("mesh");
                if (meshNode)
                {
                    primitive->mesh = assets->AssetByID<TriangleMesh>(meshNode->As<std::string>());
                }

                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region BSDF

                const auto* bsdfNode = propNode->Child("bsdf");
                if (bsdfNode)
                {
                    primitive->bsdf = assets->AssetByID<BSDF>(bsdfNode->As<std::string>());
                }

                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Add primitive

                if (primitive->id)
                {
                    primitiveIDMap_[primitive->id] = primitive.get();
                }

                primitives_.push_back(std::move(primitive));
                
                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Traverse child nodes

                const auto* childNode = propNode->Child("child");
                if (childNode)
                {
                    for (int i = 0; i < childNode->Size(); i++)
                    {
                        if (!Traverse(childNode->At(i), transform))
                        {
                            return false;
                        }
                    }
                }

                #pragma endregion

                // --------------------------------------------------------------------------------

                return true;
            };

            // --------------------------------------------------------------------------------

            const auto* nodesNode = sceneNode->Child("nodes");
            if (!nodesNode)
            {
                LM_LOG_ERROR("Missing 'nodes' node");
                PropertyUtils::PrintPrettyError(sceneNode);
                return false;
            }
            for (int i = 0; i < nodesNode->Size(); i++)
            {
                if (!Traverse(nodesNode->At(i), Mat4::Identity()))
                {
                    return false;
                }
            }

            #pragma endregion

            // --------------------------------------------------------------------------------

            #pragma region Main sensor

            {
                const auto mainSensorNode = sceneNode->Child("sensor");
                if (!mainSensorNode)
                {
                    LM_LOG_ERROR("Missing 'sensor' node");
                    PropertyUtils::PrintPrettyError(sceneNode);
                    return false;
                }

                const auto it = primitiveIDMap_.find(mainSensorNode->As<std::string>());
                if (it == primitiveIDMap_.end())
                {
                    LM_LOG_ERROR("Missing primitive ID: " + mainSensorNode->As<std::string>());
                    PropertyUtils::PrintPrettyError(mainSensorNode);
                    return false;
                }

                sensorPrimitive_ = it->second;
            }

            #pragma endregion
        }

        #pragma endregion

        // --------------------------------------------------------------------------------
        
        #pragma region Build accel

        {
            LM_LOG_INFO("Building acceleration structure");
            LM_LOG_INDENTER();

            if (accel->Build(*this))
            {
                return false;
            }

            accel_ = accel;
        }

        #pragma endregion

        // --------------------------------------------------------------------------------

        return true;
    };

    LM_IMPL_F(Intersect) = [this](const Ray& ray, Intersection& isect) -> bool
    {
        // TODO: Intersection with emitter shapes
        return accel_->Intersect(*this, ray, isect);
    };

    LM_IMPL_F(PrimitiveByID) = [this](const std::string& id) -> const Primitive*
    {
        const auto it = primitiveIDMap_.find(id);
        return it != primitiveIDMap_.end() ? it->second : nullptr;
    };

    LM_IMPL_F(Sensor) = [this]() -> const Primitive*
    {
        return sensorPrimitive_;
    };

    LM_IMPL_F(NumPrimitives) = [this]() -> int
    {
        return (int)(primitives_.size());
    };

    LM_IMPL_F(PrimitiveAt) = [this](int index) -> const Primitive*
    {
        return primitives_.at(index).get();
    };

private:

    std::vector<std::unique_ptr<Primitive>> primitives_;            // Primitives
    std::unordered_map<std::string, Primitive*> primitiveIDMap_;    // Mapping from ID to primitive pointer
    Primitive* sensorPrimitive_;                                    // Index of main sensor
    const Accel* accel_;

};

LM_COMPONENT_REGISTER_IMPL_DEFAULT(Scene_);

LM_NAMESPACE_END
