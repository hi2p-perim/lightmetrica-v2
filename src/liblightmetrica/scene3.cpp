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
#include <lightmetrica/scene3.h>
#include <lightmetrica/math.h>
#include <lightmetrica/property.h>
#include <lightmetrica/assets.h>
#include <lightmetrica/primitive.h>
#include <lightmetrica/accel3.h>
#include <lightmetrica/trianglemesh.h>
#include <lightmetrica/light.h>
#include <lightmetrica/sensor.h>
#include <lightmetrica/bsdf.h>
#include <lightmetrica/ray.h>
#include <lightmetrica/intersection.h>
#include <lightmetrica/detail/propertyutils.h>
#include <lightmetrica/detail/serial.h>

LM_NAMESPACE_BEGIN

class Scene3_ final : public Scene3
{
public:

    LM_IMPL_CLASS(Scene3_, Scene3);

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
                        primitive->id = idNode->RawScalar();
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
                                if (!matrixNode->As<Mat4>(transform))
                                {
                                    PropertyUtils::PrintPrettyError(matrixNode);
                                    return false;
                                }
                                return true;
                            }

                            // `lookat` node
                            const auto* lookatNode = transformNode->Child("lookat");
                            if (lookatNode)
                            {
                                Vec3 eye;
                                if (!lookatNode->ChildAs("eye", eye))
                                {
                                    PropertyUtils::PrintPrettyError(lookatNode);
                                    return false;
                                }
                                Vec3 center;
                                if (!lookatNode->ChildAs("center", center))
                                {
                                    PropertyUtils::PrintPrettyError(lookatNode);
                                    return false;
                                }
                                Vec3 up;
                                if (!lookatNode->ChildAs("up", up))
                                {
                                    PropertyUtils::PrintPrettyError(lookatNode);
                                    return false;
                                }

                                const auto vz = Math::Normalize(eye - center);
                                const auto vx = Math::Normalize(Math::Cross(up, vz));
                                const auto vy = Math::Cross(vz, vx);

                                transform = Mat4(
                                    vx.x, vx.y, vx.z, 0_f,
                                    vy.x, vy.y, vy.z, 0_f,
                                    vz.x, vz.y, vz.z, 0_f,
                                    eye.x, eye.y, eye.z, 1_f);

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
                                    Vec3 v;
                                    if (!translateNode->As<Vec3>(v))
                                    {
                                        PropertyUtils::PrintPrettyError(translateNode);
                                        return false;
                                    }
                                    transform *= Math::Translate(v);
                                }

                                // Parse 'rotate' node
                                if (rotateNode)
                                {
                                    Float angle;
                                    if (!rotateNode->ChildAs("angle", angle))
                                    {
                                        PropertyUtils::PrintPrettyError(rotateNode);
                                        return false;
                                    }
                                    Vec3 axis;
                                    if (!rotateNode->ChildAs("axis", axis))
                                    {
                                        PropertyUtils::PrintPrettyError(rotateNode);
                                        return false;
                                    }
                                    transform *= Math::Rotate(Math::Radians(angle), axis);
                                }

                                // Parse 'scale' node
                                if (scaleNode)
                                {
                                    Vec3 v;
                                    if (!scaleNode->As<Vec3>(v))
                                    {
                                        PropertyUtils::PrintPrettyError(scaleNode);
                                        return false;
                                    }
                                    transform *= Math::Scale(v);
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

                    // Compute normal transform
                    primitive->normalTransform = Mat3(Math::Transpose(Math::Inverse(primitive->transform)));
                }
                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region Triangle mesh
                const auto* meshNode = propNode->Child("mesh");
                if (meshNode)
                {
                    primitive->mesh = static_cast<const TriangleMesh*>(assets->AssetByIDAndType(meshNode->RawScalar(), "trianglemesh", primitive.get()));
                }
                #pragma endregion

                // --------------------------------------------------------------------------------

                #pragma region BSDF
                const auto* bsdfNode = propNode->Child("bsdf");
                if (bsdfNode)
                {
                    primitive->bsdf = static_cast<const BSDF*>(assets->AssetByIDAndType(bsdfNode->RawScalar(), "bsdf", primitive.get()));
                }
                else
                {
                    // If bsdf node is empty, assign 'null' bsdf.
                    primitive->bsdf = nullBSDF_.get();
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
                        primitive->light   = static_cast<const Light*>(assets->AssetByIDAndType(L->RawScalar(), "light", primitive.get()));
                        primitive->emitter = static_cast<const Emitter*>(primitive->light);
                        lightPrimitiveIndices_.push_back(primitives_.size());
                    }
                    else if (E)
                    {
                        primitive->sensor  = static_cast<const Sensor*>(assets->AssetByIDAndType(E->RawScalar(), "sensor", primitive.get()));
                        primitive->emitter = static_cast<const Emitter*>(primitive->sensor);
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

                #pragma region Add primitive
                // Register primitive ID
                primitive->index = primitives_.size();
                if (primitive->id)
                {
                    primitiveIDMap_[primitive->id] = primitive->index;
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

                const auto it = primitiveIDMap_.find(mainSensorNode->RawScalar());
                if (it == primitiveIDMap_.end())
                {
                    LM_LOG_ERROR("Missing primitive ID: " + mainSensorNode->As<std::string>());
                    PropertyUtils::PrintPrettyError(mainSensorNode);
                    return false;
                }

                sensorPrimitiveIndex_ = it->second;
            }
            #pragma endregion
        }

        #pragma endregion

        // --------------------------------------------------------------------------------

        #pragma region Compute scene bound
        // AABB
        bound_ = Bound();
        for (const auto& primitive : primitives_)
        {
            if (primitive->mesh)
            {
                const int n = primitive->mesh->NumVertices();
                const auto* ps = primitive->mesh->Positions();
                for (int i = 0; i < n; i++)
                {
                    Vec3 p(primitive->transform * Vec4(ps[3 * i], ps[3 * i + 1], ps[3 * i + 2], 1_f));
                    bound_ = Math::Union(bound_, p);
                }
            }

            if (primitive->emitter && primitive->emitter->GetBound.Implemented())
            {
                bound_ = Math::Union(bound_, primitive->emitter->GetBound());
            }
        }
        
        // Bounding sphere
        sphereBound_.center = (bound_.max + bound_.min) * .5_f;
        sphereBound_.radius = Math::Length(sphereBound_.center - bound_.max) * 1.01_f;  // Grow slightly
        #pragma endregion

        // --------------------------------------------------------------------------------

        #pragma region Post load
        if (!assets->PostLoad(this))
        {
            return false;
        }
        #pragma endregion

        // --------------------------------------------------------------------------------

        #pragma region Create emitter shapes
        for (const auto& primitive : primitives_)
        {
            if (primitive->emitter && primitive->emitter->GetEmitterShape.Implemented())
            {
                emitterShapes_.push_back(primitive->emitter->GetEmitterShape());
            }
        }
        #pragma endregion

        // --------------------------------------------------------------------------------
        
        #pragma region Build accel
        {
            LM_LOG_INFO("Building acceleration structure");
            LM_LOG_INDENTER();
            auto* accel3 = static_cast<Accel3*>(accel);
            if (!accel3->Build(this))
            {
                return false;
            }
            accel_ = accel3;
        }
        #pragma endregion

        // --------------------------------------------------------------------------------

        return true;
    };

    LM_IMPL_F(Intersect) = [this](const Ray& ray, Intersection& isect) -> bool
    {
        // Intersect with accel
        bool hit = accel_->Intersect(this, ray, isect, Math::EpsIsect(), Math::Inf());

        // Intersect with emitter shapes
        if (!hit)
        {
            Float maxT = Math::Inf();
            for (size_t i = 0; i < emitterShapes_.size(); i++)
            {
                if (emitterShapes_[i]->Intersect(ray, Math::EpsIsect(), maxT, isect))
                {
                    maxT = Math::Length(isect.geom.p - ray.o);
                    hit = true;
                }
            }
        }
        
        return hit;
    };

    LM_IMPL_F(IntersectWithRange) = [this](const Ray& ray, Intersection& isect, Float minT, Float maxT) -> bool
    {
        return accel_->Intersect(this, ray, isect, minT, maxT);
    };

    LM_IMPL_F(PrimitiveByID) = [this](const std::string& id) -> const Primitive*
    {
        const auto it = primitiveIDMap_.find(id);
        return it != primitiveIDMap_.end() ? primitives_.at(it->second).get() : nullptr;
    };

    LM_IMPL_F(GetSensor) = [this]() -> const Primitive*
    {
        return primitives_.at(sensorPrimitiveIndex_).get();
    };

    LM_IMPL_F(NumPrimitives) = [this]() -> int
    {
        return (int)(primitives_.size());
    };

    LM_IMPL_F(PrimitiveAt) = [this](int index) -> const Primitive*
    {
        return primitives_.at(index).get();
    };

    LM_IMPL_F(SampleEmitter) = [this](int type, Float u) -> const Primitive*
    {
        if ((type & SurfaceInteractionType::L) > 0)
        {
            int n = static_cast<int>(lightPrimitiveIndices_.size());
            int i = Math::Clamp(static_cast<int>(u * n), 0, n - 1);
            return primitives_.at(lightPrimitiveIndices_[i]).get();
        }

        if ((type & SurfaceInteractionType::E) > 0)
        {
            return primitives_.at(sensorPrimitiveIndex_).get();
        }

        LM_UNREACHABLE();
        return nullptr;
    };

    LM_IMPL_F(EvaluateEmitterPDF) = [this](const Primitive* primitive) -> PDFVal
    {
        if ((primitive->emitter->Type() & SurfaceInteractionType::L) > 0)
        {
            const int n = static_cast<int>(lightPrimitiveIndices_.size());
            return PDFVal(PDFMeasure::Discrete, 1_f / Float(n));
        }

        if ((primitive->emitter->Type() & SurfaceInteractionType::E) > 0)
        {
            return PDFVal(PDFMeasure::Discrete, 1_f);
        }

        LM_UNREACHABLE();
        return PDFVal(PDFMeasure::Discrete, 0_f);
    };

    LM_IMPL_F(GetBound) = [this]() -> Bound
    {
        return bound_;
    };

    LM_IMPL_F(GetSphereBound) = [this]() -> SphereBound
    {
        return sphereBound_;
    };

    LM_IMPL_F(Serialize) = [this]() -> std::string
    {
        // Convert primitives to serializable format
        std::vector<SerializablePrimitive> serializablePrimitives;
        for (const auto& p : primitives_)
        {
            serializablePrimitives.emplace_back(p);
        }

        // Serialize into binary
        std::stringstream ss;
        std::basic_ifstream<unsigned char> out()
        {

            cereal::PortableBinaryOutputArchive oa()
            oa(serializablePrimitives);
            oa(primitiveIDMap_);
            oa(sensorPrimitiveIndex_);
            oa(lightPrimitiveIndices_);
        }
        return ss.str();
    };

    LM_IMPL_F(Deserialize) = [this](const std::string& serialized, const std::unordered_map<std::string, void*>& userdata) -> void
    {
        
    };

private:

    std::vector<std::unique_ptr<Primitive>> primitives_;                // Primitives
    std::unordered_map<std::string, size_t> primitiveIDMap_;            // Mapping from ID to primitive index
    size_t sensorPrimitiveIndex_;                                       // Sensor primitive index
    std::vector<size_t> lightPrimitiveIndices_;                         // Pointers to light primitives

    const Accel3* accel_;                                               // Acceleration structure
    Bound bound_;                                                       // Scene bound (AABB)
    SphereBound sphereBound_;                                           // Scene bound (sphere)
    std::vector<const EmitterShape*> emitterShapes_;                    // Special shapes for emitters

    // Predefined assets
    BSDF::UniquePtr nullBSDF_ = ComponentFactory::Create<BSDF>("bsdf::null");

};

LM_COMPONENT_REGISTER_IMPL(Scene3_, "scene::scene3");

LM_NAMESPACE_END
