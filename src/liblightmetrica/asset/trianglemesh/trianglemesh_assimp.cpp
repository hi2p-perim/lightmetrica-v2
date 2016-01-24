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
#include <lightmetrica/trianglemesh.h>
#include <lightmetrica/property.h>
#include <lightmetrica/logger.h>

#include <assimp/Importer.hpp>
#include <assimp/DefaultLogger.hpp>
#include <assimp/LogStream.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

LM_NAMESPACE_BEGIN

class LogStream final : public Assimp::LogStream
{
public:

    LogStream(LogType type) : Type(type) {}

    virtual void write(const char* message) override
    {
        // Remove new line
        std::string str(message);
        str.erase(std::remove(str.begin(), str.end(), '\n'), str.end());

        // Remove initial string
        std::regex re("[a-zA-Z]+, +T[0-9]+: (.*)");
        str = "Assimp : " + std::regex_replace(str, re, "$1");

        switch (Type)
        {
            case LogType::Error: { LM_LOG_ERROR(str); break; }
            case LogType::Warn:  { LM_LOG_WARN(str); break; }
            case LogType::Info:  { LM_LOG_INFO(str); break; }
            case LogType::Debug: { LM_LOG_DEBUG(str); break; }
        }
    }

private:

    LogType Type;

};

class TriangleMesh_Assimp : public TriangleMesh
{
public:

    LM_IMPL_CLASS(TriangleMesh_Assimp, TriangleMesh);

public:

    LM_IMPL_F(Load) = [this](const PropertyNode* prop, Assets* assets, const Primitive* primitive) -> bool
    {
        #pragma region Assimp logger

		Assimp::DefaultLogger::create("", Assimp::Logger::VERBOSE);
		Assimp::DefaultLogger::get()->attachStream(new LogStream(LogType::Info), Assimp::Logger::Info);
		Assimp::DefaultLogger::get()->attachStream(new LogStream(LogType::Warn), Assimp::Logger::Warn);
		Assimp::DefaultLogger::get()->attachStream(new LogStream(LogType::Error), Assimp::Logger::Err);
		#if NGI_DEBUG_MODE
		Assimp::DefaultLogger::get()->attachStream(new LogStream(LogType::Debug), Assimp::Logger::Debugging);
		#endif

		#pragma endregion

        // --------------------------------------------------------------------------------

        #pragma region Load scene

        //const auto meshNode = primitiveNode["mesh"];
        //const auto postProcessNode = meshNode["postprocess"];

        const auto localpath = prop->Child("path")->As<std::string>();
        const auto basepath = boost::filesystem::path(prop->Tree()->Path()).parent_path();
        const auto path = basepath / localpath;

        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(path.string().c_str(), 0);

        if (!scene)
        {
            LM_LOG_ERROR(importer.GetErrorString());
            return false;
        }

        if (scene->mNumMeshes == 0)
        {
            LM_LOG_ERROR("No mesh is found in " + localpath);
            return false;
        }

        //if (!scene->mMeshes[0]->HasNormals() && postProcessNode)
        if (!scene->mMeshes[0]->HasNormals())
        {
            importer.ApplyPostProcessing(
                aiProcess_GenNormals |
                //(postProcessNode["generate_normals"].as<bool>() ? aiProcess_GenNormals : 0) |
                //(postProcessNode["generate_smooth_normals"].as<bool>() ? aiProcess_GenSmoothNormals : 0) |
                aiProcess_Triangulate |
                aiProcess_JoinIdenticalVertices |
                aiProcess_PreTransformVertices);
        }
        else
        {
            importer.ApplyPostProcessing(
                aiProcess_Triangulate |
                aiProcess_JoinIdenticalVertices |
                aiProcess_PreTransformVertices);
        }

        #pragma endregion

        // --------------------------------------------------------------------------------

        #pragma region Load triangle mesh

        {
            const auto* aimesh = scene->mMeshes[0];

            // --------------------------------------------------------------------------------

            #pragma region Positions and normals

            for (unsigned int i = 0; i < aimesh->mNumVertices; i++)
            {
                auto& p = aimesh->mVertices[i];
                auto& n = aimesh->mNormals[i];
                ps_.push_back(Float(p.x));
                ps_.push_back(Float(p.y));
                ps_.push_back(Float(p.z));
                ns_.push_back(Float(n.x));
                ns_.push_back(Float(n.y));
                ns_.push_back(Float(n.z));
            }

            #pragma endregion

            // --------------------------------------------------------------------------------

            #pragma region Texture coordinates

            if (aimesh->HasTextureCoords(0))
            {
                for (unsigned int i = 0; i < aimesh->mNumVertices; i++)
                {
                    auto& uv = aimesh->mTextureCoords[0][i];
                    ts_.push_back(Float(uv.x));
                    ts_.push_back(Float(uv.y));
                }
            }

            #pragma endregion

            // --------------------------------------------------------------------------------

            #pragma region Faces

            for (unsigned int i = 0; i < aimesh->mNumFaces; i++)
            {
                // The mesh is already triangulated
                auto& f = aimesh->mFaces[i];
                fs_.push_back(f.mIndices[0]);
                fs_.push_back(f.mIndices[1]);
                fs_.push_back(f.mIndices[2]);
            }

            #pragma endregion
        }

        #pragma endregion

        // --------------------------------------------------------------------------------

        return true;
    };

public:

    LM_IMPL_F(NumVertices) = [this]() -> int { return (int)(ps_.size()) / 3; };
    LM_IMPL_F(NumFaces)    = [this]() -> int { return (int)(fs_.size()) / 3; };
    LM_IMPL_F(Positions)   = [this]() -> const Float* { return ps_.data(); };
    LM_IMPL_F(Normals)     = [this]() -> const Float* { return ns_.data(); };
    LM_IMPL_F(Texcoords)   = [this]() -> const Float* { return ts_.data(); };
    LM_IMPL_F(Faces)       = [this]() -> const unsigned int* { return fs_.data(); };

protected:

    std::vector<Float> ps_;
    std::vector<Float> ns_;
    std::vector<Float> ts_;
    std::vector<unsigned int> fs_;

};

LM_COMPONENT_REGISTER_IMPL(TriangleMesh_Assimp, "trianglemesh::assimp");

LM_NAMESPACE_END
