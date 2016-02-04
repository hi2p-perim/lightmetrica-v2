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
#include <lightmetrica/component.h>
#include <lightmetrica/logger.h>

LM_NAMESPACE_BEGIN

struct CreateAndReleaseFuncs
{
    CreateFuncPointerType createFunc;
    ReleaseFuncPointerType releaseFunc;
};

/*
    Implementation of ComponentFactory.
    Note that in this class the error message cannot be output
    with logger framework (e.g., using LM_LOG_ERROR, etc.)
    because the registration of creation/release function is processed 
    in static initialization phase, so LM_LOG_INIT is not yet be called.
*/
class ComponentFactoryImpl
{
public:

    static ComponentFactoryImpl& Instance()
    {
        static ComponentFactoryImpl instance;
        return instance;
    }

public:

    auto Register(const std::string& key, CreateFuncPointerType createFunc, ReleaseFuncPointerType releaseFunc) -> void
    {
        // Check if already registered
        if (funcMap.find(key) != funcMap.end())
        {
            // Note that in this class the error message cannot be output
            // with logger framework(e.g., using LM_LOG_ERROR, etc.)
            // because the registration of creation / release function is processed
            // in static initialization phase, so LM_LOG_INIT is not yet be called.
            std::cerr << "Failed to register [ " << key << " ]. Already registered." << std::endl;
        }

        #if 0
        std::cout << "Registering: " << key << std::endl;
        #endif

        funcMap[key] = CreateAndReleaseFuncs{createFunc, releaseFunc};
    }

    auto Create(const char* key) -> Component*
    {
        auto it = funcMap.find(key);
        if (it == funcMap.end())
        {
            return nullptr;
        }

        auto* p = it->second.createFunc();
        p->createFunc = it->second.createFunc;
        p->releaseFunc = it->second.releaseFunc;
        return p;
    }

    auto ReleaseFunc(const char* key) -> ReleaseFuncPointerType
    {
        auto it = funcMap.find(key);
        return it == funcMap.end() ? nullptr : it->second.releaseFunc;
    }

    auto LoadPlugin(const std::string& path) -> bool
    {
        LM_LOG_INFO("Loading '" + path + "'");
        LM_LOG_INDENTER();

        // Load plugin
        std::unique_ptr<DynamicLibrary> plugin(new DynamicLibrary);
        if (!plugin->Load(path))
        {
            LM_LOG_WARN("Failed to load library: " + path);
            return false;
        }

        plugins.push_back(std::move(plugin));

        LM_LOG_INFO("Successfully loaded");
        return true;
    }

    auto LoadPlugins(const std::string& directory) -> void
    {
        namespace fs = boost::filesystem;

        // Skip if directory does not exist
        if (!fs::is_directory(fs::path(directory)))
        {
            LM_LOG_WARN("Missing plugin directory '" + directory + "'. Skipping.");
            return;
        }

        // File format
        #if LM_PLATFORM_WINDOWS
        const std::regex pluginNameExp("([a-z_]+)\\.dll$");
        #elif LM_PLATFORM_LINUX
        const std::regex pluginNameExp("^([a-z_]+)\\.so$");
        #elif LM_PLATFORM_APPLE
        const std::regex pluginNameExp("^([a-z_]+)\\.dylib$");
        #endif

        // Enumerate dynamic libraries in #pluginDir
        fs::directory_iterator endIter;
        for (fs::directory_iterator it(directory); it != endIter; ++it)
        {
            if (fs::is_regular_file(it->status()))
            {
                std::cmatch match;
                auto filename = it->path().filename().string();
                if (std::regex_match(filename.c_str(), match, pluginNameExp))
                {
                    if (!LoadPlugin(fs::change_extension(it->path(), "").string()))
                    {
                        continue;
                    }
                }
            }
        }
    }

    auto UnloadPlugins() -> void
    {
        for (auto& plugin : plugins)
        {
            plugin->Unload();
        }

        plugins.clear();
    }

private:

    // Registered implementations
    using FuncMap = std::unordered_map<std::string, CreateAndReleaseFuncs>;
    FuncMap funcMap;

    // Loaded plugins
    std::vector<std::unique_ptr<DynamicLibrary>> plugins;

};

auto ComponentFactory_Register(const char* key, CreateFuncPointerType createFunc, ReleaseFuncPointerType releaseFunc) -> void { ComponentFactoryImpl::Instance().Register(key, createFunc, releaseFunc); }
auto ComponentFactory_Create(const char* key) -> Component* { return ComponentFactoryImpl::Instance().Create(key); }
auto ComponentFactory_ReleaseFunc(const char* key) -> ReleaseFuncPointerType { return ComponentFactoryImpl::Instance().ReleaseFunc(key); }
auto ComponentFactory_LoadPlugin(const char* path) -> bool { return ComponentFactoryImpl::Instance().LoadPlugin(path); }
auto ComponentFactory_LoadPlugins(const char* directory) -> void { ComponentFactoryImpl::Instance().LoadPlugins(directory); }
auto ComponentFactory_UnloadPlugins() -> void { ComponentFactoryImpl::Instance().UnloadPlugins(); }

LM_NAMESPACE_END