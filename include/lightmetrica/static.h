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
#endif

LM_NAMESPACE_BEGIN

// --------------------------------------------------------------------------------

/*
    Dynamic library.

    TODO: Multiplatform.
*/
class DynamicLibrary
{
public:

    auto Load(const std::string& path) -> bool
    {
        handle = LoadLibraryA(path.c_str());
        if (!handle)
        {
            std::cerr << "Failed to load library : " << path << std::endl;
            std::cerr << GetLastErrorAsString() << std::endl;
            return false;
        }
        return true;
    }

    auto GetFuncPointer(const std::string& symbol) const -> void*
    {
        void* address = GetProcAddress(handle, symbol.c_str());
        if (address == nullptr)
        {
            std::cerr << "Failed to get address of '" << symbol << "'" << std::endl;
            std::cerr << GetLastErrorAsString() << std::endl;
            return nullptr;
        }

        return address;
    }

private:

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

public:

    HMODULE handle;

};

// --------------------------------------------------------------------------------

struct InternalPolicy {};
struct ExternalPolicy {};

#ifdef LM_EXPORTS
using InitPolicy = InternalPolicy;
#else
using InitPolicy = ExternalPolicy;
#endif

/*
    Static initialization.
    
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
        if (!lib->Load("liblightmetrica.dll"))
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
    #define LM_EXPORTED_F(Func, ...) return Func(__VA_ARGS__);
#else
    #define LM_EXPORTED_F(Func, ...) \
        using FuncPtrType = decltype(&Func); \
        static auto func = []() -> FuncPtrType { \
            const auto* lib = StaticInit<ExternalPolicy>::Instance().Library(); \
            const auto* f = static_cast<FuncPtrType>(lib->GetFuncPointer(#Func)); \
            if (!f) std::exit(EXIT_FAILURE); \
            return f; \
        }(); \
        return func(__VA_ARGS__);
#endif

LM_NAMESPACE_END
