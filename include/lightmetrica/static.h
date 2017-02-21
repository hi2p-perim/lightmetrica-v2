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

#pragma once

#include <lightmetrica/macros.h>

#include <iostream>
#include <string>
#include <memory>
#if LM_PLATFORM_WINDOWS
#include <Windows.h>
#elif LM_PLATFORM_LINUX || LM_PLATFORM_APPLE
#include <dlfcn.h>
#endif

LM_NAMESPACE_BEGIN

/*!
    \addtogroup core
    \{
*/

// --------------------------------------------------------------------------------

/*!
    \brief Dynamic library.

    Platform independent dynamic library class.
*/
class DynamicLibrary
{
public:

    /*!
        \brief Load a dynamic library.
        \param path Path to a library file.
        \retval true Succeeded to load.
        \retval false Failed to load.
    */
    auto Load(const std::string& path) -> bool
    {
        #if LM_PLATFORM_WINDOWS
        const auto p = path + ".dll";
        #elif LM_PLATFORM_LINUX
        const auto p = path + ".so";
        #elif LM_PLATFORM_APPLE
        const auto p = path + ".dylib";
        #endif

        #if LM_PLATFORM_WINDOWS
        handle = LoadLibraryA(p.c_str());
        if (!handle)
        {
            std::cerr << "Failed to load library or its dependencies : " << p << std::endl;
            std::cerr << GetLastErrorAsString() << std::endl;
            return false;
        }
        #elif LM_PLATFORM_LINUX || LM_PLATFORM_APPLE
        handle = dlopen(p.c_str(), RTLD_LAZY | RTLD_LOCAL);
        if (!handle)
        {
            std::cerr << "Failed to load library or its dependencies : " << p << std::endl;
            std::cerr << dlerror() << std::endl;
            return false;
        }
        #endif

        return true;
    }

    /*!
        \brief Unload the dynamic library.
        \retval true Succeeded to unload.
        \retval false Failed to unload.
    */
    auto Unload() -> bool
    {
        #if LM_PLATFORM_WINDOWS
        if (!FreeLibrary(handle))
        {
            std::cerr << "Failed to free library" << std::endl;
            std::cerr << GetLastErrorAsString() << std::endl;
            return false;
        }
        #elif LM_PLATFORM_LINUX || LM_PLATFORM_APPLE
        if (dlclose(handle) != 0)
        {
            std::cerr << "Failed to free library" << std::endl;
            std::cerr << dlerror() << std::endl;
            return false;
        }
        #endif

        return true;
    }

    /*!
        \brief Retrieve the address of an exported symbol.
        \retval nullptr Failed to get address.
    */
    auto GetFuncPointer(const std::string& symbol) const -> void*
    {
        #if LM_PLATFORM_WINDOWS
        void* address = (void*)GetProcAddress(handle, symbol.c_str());
        if (address == nullptr)
        {
            std::cerr << "Failed to get address of '" << symbol << "'" << std::endl;
            std::cerr << GetLastErrorAsString() << std::endl;
            return nullptr;
        }
        #elif LM_PLATFORM_LINUX || LM_PLATFORM_APPLE
        void* address = dlsym(handle, symbol.c_str());
        if (address == nullptr)
        {
            std::cerr << "Failed to get address of '" << symbol << "'" << std::endl;
            std::cerr << dlerror() << std::endl;
            return nullptr;
        }
        #endif

        return address;
    }

private:

    #if LM_PLATFORM_WINDOWS
    auto GetLastErrorAsString() const -> std::string
    {
        DWORD error = GetLastError();
        if (error == 0)
        {
            return std::string();
        }

        LPSTR buffer = nullptr;
        size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&buffer, 0, NULL);
        std::string message(buffer, size);
        LocalFree(buffer);

        return message;
    }
    #endif

public:

    #if LM_PLATFORM_WINDOWS
	HMODULE handle;
    #elif LM_PLATFORM_LINUX || LM_PLATFORM_APPLE
    void* handle;
    #endif

};

// --------------------------------------------------------------------------------

struct InternalPolicy {};
struct ExternalPolicy {};

#ifdef LM_EXPORTS
using InitPolicy = InternalPolicy;
#else
using InitPolicy = ExternalPolicy;
#endif

/*!
    \brief Static initialization.
    
    Performs a static initialization using
    the technique to combine static member function and singleton
    using this technique, the process in the constructor is called once.

    \tparam InitPolicy
        Controls initialization policy.
        If the StaticInit class is specified with `ExternalPolicy`,
        the initialization is dedicated to the load from external source.
*/
template <typename InitPolicy>
class StaticInit;

template <>
class StaticInit<InternalPolicy>
{
public:

    static auto Instance() -> StaticInit&
    {
        static StaticInit inst;
        return inst;
    }

    StaticInit() {}

};

template <>
class StaticInit<ExternalPolicy>
{
public:

    static auto Instance() -> StaticInit&
    {
        static StaticInit inst;
        return inst;
    }

    StaticInit()
        : lib(new DynamicLibrary)
    {
        // Load DLL
        // Assume in the dynamic library is in the same directory as executable
        // TODO: search from search paths.
        if (!lib->Load("liblightmetrica"))
        {
            // This is fatal error, the application should exit immediately
            std::exit(EXIT_FAILURE);
        }
    }

    auto Library() const -> const DynamicLibrary*
    {
        return lib.get();
    }

private:

    std::unique_ptr<DynamicLibrary> lib;

};

namespace
{
    struct StaticInitReg { static const StaticInit<InitPolicy>& reg; };
    const StaticInit<InitPolicy>& StaticInitReg::reg = StaticInit<InitPolicy>::Instance();
}

// --------------------------------------------------------------------------------

/*
    LM_EXPORTED_F
    TODO
      - Thread safety
*/

#ifdef LM_EXPORTS
    #define LM_EXPORTED_F(Func, ...) Func(__VA_ARGS__)
#else
    #define LM_EXPORTED_F(Func, ...) \
        [&]() { \
            using FuncPtrType = decltype(&Func); \
            static auto func = []() -> FuncPtrType { \
                const auto* lib = StaticInit<ExternalPolicy>::Instance().Library(); \
                const FuncPtrType f = reinterpret_cast<FuncPtrType>(lib->GetFuncPointer(#Func)); \
                if (!f) std::exit(EXIT_FAILURE); \
                return f; \
            }(); \
            return func(__VA_ARGS__); \
        }()
#endif

//! \}

LM_NAMESPACE_END
