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
#include <lightmetrica/portable.h>
#include <lightmetrica/reflection.h>
#include <lightmetrica/static.h>
#include <lightmetrica/logger.h>
#include <lightmetrica/metacounter.h>

#include <functional>
#include <type_traits>
#include <memory>
#include <string>

LM_NAMESPACE_BEGIN

#pragma region Component

/*!
    Component.

    The base class for all component classes.
    The component system of lightmetrica is main feature for
    retaining portability of the framework.
    For technical details, see <TODO>.
*/
struct Component
{
    // VTable entries and user-defined data
    // User-defined data is required to hold non-portable version
    static constexpr size_t VTableNumEntries = 100;
    struct VTableEntry
    {
        void* f     = nullptr;
        void* implf = nullptr;
    } vt_[VTableNumEntries];

    // Name of implementation type
    const char* implName = nullptr;

    // Number of interface functions
    static constexpr int NumFuncs = 0;
};

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Interface definition

template <int ID, typename Iface, typename Signature>
struct VirtualFunction;

template <int ID, typename Iface, typename ReturnType, typename ...ArgTypes>
struct VirtualFunction<ID, Iface, ReturnType(ArgTypes...)>
{
    using Type = ReturnType(ArgTypes...);
    Component* o_;
    const char* name_;
    explicit VirtualFunction<ID, Iface, ReturnType(ArgTypes...)>(Component* o, const char* name) : o_(o), name_(name) {};
    auto operator()(ArgTypes... args) const -> ReturnType
    {
        // Convert argument types to portable types and call the functions stored in the vtable.
        using FuncType = Portable<ReturnType>(*)(void*, Portable<ArgTypes>...);
        if (!o_->vt_[ID].f)
        {
            LM_LOG_ERROR("Missing vtable entry for");
            {
                LM_LOG_INDENTER();
                LM_LOG_ERROR("Interface: " + std::string(Iface::Type_().name));
                LM_LOG_ERROR("Instance : " + std::string(o_->implName));
                LM_LOG_ERROR("Function : " + std::string(name_) + " (ID: " + std::to_string(ID) + ")");
            }
            LM_LOG_ERROR("Possible cause of this error:");
            {
                LM_LOG_INDENTER();
                LM_LOG_ERROR("Missing implementation. We recommend to "
                             "check if the function '" + std::string(o_->implName) + "::" + std::string(name_) + "' is properly implmeneted with RF_IMPL_F macro.");
            }
            return ReturnType();
        }
        return reinterpret_cast<FuncType>(o_->vt_[ID].f)(o_->vt_[ID].implf, Portable<ArgTypes>(args)...).Get();
    }
};

template <int ID, typename Iface, typename Signature>
struct VirtualFunctionGenerator;

template <int ID, typename Iface, typename ReturnType, typename ...ArgTypes>
struct VirtualFunctionGenerator<ID, Iface, ReturnType(ArgTypes...)>
{
    static_assert(ID < Component::VTableNumEntries, "Excessive vtable entries");
    using VirtualFunctionType = VirtualFunction<ID, Iface, ReturnType(ArgTypes...)>;
    static auto Get(Component* o, const char* name) -> VirtualFunctionType
    {
        return VirtualFunctionType(o, name);
    }
};

// Define interface class
#define LM_INTERFACE_CLASS(Current, Base) \
        LM_DEFINE_CLASS_TYPE(Current, Base); \
        using BaseType = Base; \
        using InterfaceType = Current; \
        using UniquePtr = std::unique_ptr<InterfaceType, void(*)(Component*)>

// Define interface member function
#if LM_INTELLISENSE
    // Nasty workaround for intellisense
    #define LM_INTERFACE_F(Name, Signature) \
            const std::function<Signature> Name
#else
    #define LM_INTERFACE_F(Name, Signature) \
            static constexpr int Name ## _ID_ = MetaCounter<BaseType>::Value() + MetaCounter<InterfaceType>::Next() - 1; \
            const VirtualFunction<Name ## _ID_, InterfaceType, Signature> Name = VirtualFunctionGenerator<Name ## _ID_, InterfaceType, Signature>::Get(this, #Name)
#endif

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Implementation definition

template <typename Signature>
struct ImplFunctionGenerator;

template <typename ReturnType, typename ...ArgTypes>
struct ImplFunctionGenerator<ReturnType(ArgTypes...)>
{
    using PortableFunctionType = Portable<ReturnType>(*)(void*, Portable<ArgTypes>...);
    static auto Get() -> PortableFunctionType
    {
        return static_cast<PortableFunctionType>(
            [](void* userdata, Portable<ArgTypes>... args) -> Portable<ReturnType>
            {
                // Convert user defined implementation to original type
                using UserFunctionType = std::function<ReturnType(ArgTypes...)>;
                auto& f = *reinterpret_cast<UserFunctionType*>(userdata);
                return Portable<ReturnType>(f((args.Get())...));
            });
    }
};

template <typename ...ArgTypes>
struct ImplFunctionGenerator<void(ArgTypes...)>
{
    using PortableFunctionType = Portable<void>(*)(void*, Portable<ArgTypes>...);
    static auto Get() -> PortableFunctionType
    {
        return static_cast<PortableFunctionType>(
            [](void* userdata, Portable<ArgTypes>... args) -> Portable<void>
        {
            // Convert user defined implementation to original type
            using UserFunctionType = std::function<void(ArgTypes...)>;
            auto& f = *reinterpret_cast<UserFunctionType*>(userdata);
            f((args.Get())...);
            return Portable<void>();
        });
    }
};

#define LM_IMPL_CLASS(Impl, Base) \
    LM_DEFINE_CLASS_TYPE(Impl, Base); \
    using ImplType = Impl; \
    using BaseType = Base; \
    const struct Impl ## _Init_ { \
        Impl ## _Init_(ImplType* p) { p->implName = ImplType::Type_().name; } \
    } Impl ## _Init_Inst_{this}

#if LM_INTELLISENSE
    #define LM_IMPL_F(Name) \
        const std::function<decltype(BaseType::Name)::Type> Name ## _Impl_ 
#else
    #define LM_IMPL_F(Name) \
        struct Name ## _Init_ { \
            Name ## _Init_(ImplType* p) { \
                p->vt_[Name ## _ID_].f     = (void*)(ImplFunctionGenerator<decltype(BaseType::Name)::Type>::Get()); \
                p->vt_[Name ## _ID_].implf = (void*)(&p->Name ## _Impl_); \
            } \
        } Name ## _Init_Inst_{this}; \
        friend struct Name ## _Init_; \
        const std::function<decltype(BaseType::Name)::Type> Name ## _Impl_ 
#endif

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Component factory

using CreateFuncPointerType = Component* (*)();
using ReleaseFuncPointerType = void(*)(Component*);

/*!
    Component factory.

    Instance factory class for component creation.
    All components are instanciated with this class.
*/

extern "C"
{
    LM_PUBLIC_API auto ComponentFactory_Register(const char* implName, CreateFuncPointerType createFunc, ReleaseFuncPointerType releaseFunc) -> void;
    LM_PUBLIC_API auto ComponentFactory_Create(const char* implName)->Component*;
    LM_PUBLIC_API auto ComponentFactory_ReleaseFunc(const char* implName)->ReleaseFuncPointerType;
}

class ComponentFactory
{
public:

    LM_DISABLE_CONSTRUCT(ComponentFactory);

public:

    static auto Register(const std::string& implName, CreateFuncPointerType createFunc, ReleaseFuncPointerType releaseFunc) -> void { LM_EXPORTED_F(ComponentFactory_Register, implName.c_str(), createFunc, releaseFunc); }
    static auto Create(const std::string& implName) -> Component* { LM_EXPORTED_F(ComponentFactory_Create, implName.c_str()); }
    static auto ReleaseFunc(const std::string& implName) -> ReleaseFuncPointerType { LM_EXPORTED_F(ComponentFactory_ReleaseFunc, implName.c_str()); }

public:

    template <typename InterfaceType>
    static auto Create(const std::string& implName) -> std::unique_ptr<InterfaceType, ReleaseFuncPointerType>
    {
        using ReturnType = std::unique_ptr<InterfaceType, ReleaseFuncPointerType>;
        auto* p = static_cast<InterfaceType*>(Create(implName));
        if (!p)
        {
            LM_LOG_ERROR("Failed to create instance");
            LM_LOG_INDENTER();
            LM_LOG_ERROR("Impl: " + implName);
            LM_LOG_ERROR("Interface: " + std::string(InterfaceType::Type_().name));
            return ReturnType(nullptr, nullptr);
        }
        return ReturnType(p, ReleaseFunc(implName));
    }

    template <typename InterfaceType>
    static auto Create() -> std::unique_ptr<InterfaceType, ReleaseFuncPointerType>
    {
        const auto implName = std::string(InterfaceType::Type_().name) + "_";
        return Create<InterfaceType>(implName);
    }

};

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Helper functions

namespace
{
    // http://stackoverflow.com/questions/21174593/downcasting-unique-ptrbase-to-unique-ptrderived
    template <typename Derived, typename Base, typename Del>
    auto static_unique_ptr_cast(std::unique_ptr<Base, Del>&& p) -> std::unique_ptr<Derived, Del>
    {
        auto d = static_cast<Derived*>(p.release());
        return std::unique_ptr<Derived, Del>(d, std::move(p.get_deleter()));
    }

    template <typename Derived, typename Base, typename Del>
    auto dynamic_unique_ptr_cast(std::unique_ptr<Base, Del>&& p) -> std::unique_ptr<Derived, Del>
    {
        if (Derived *result = dynamic_cast<Derived*>(p.get()))
        {
            p.release();
            return std::unique_ptr<Derived, Del>(result, std::move(p.get_deleter()));
        }
        return std::unique_ptr<Derived, Del>(nullptr, p.get_deleter());
    }
}

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Implementation registry

/*!
    Implementation entry.

    The constructor is called automatically and registers
    instance creation function to the component factory.

    \tparam ImplType Implementation class type.
*/
template <typename ImplType>
class ImplEntry
{
public:

    static auto Instance(const std::string& implName) -> ImplEntry<ImplType>&
    {
        static ImplEntry<ImplType> instance(implName);
        return instance;
    }

private:

    ImplEntry(const std::string& implName)
    {
        ComponentFactory::Register(
            implName,
            []() -> Component* { return new ImplType; },
            [](Component* p) -> void
            {
                auto* p2 = static_cast<ImplType*>(p);
                LM_SAFE_DELETE(p2);
                p = nullptr;
            });
    }

};

#if LM_INTELLISENSE
    #define LM_COMPONENT_REGISTER_IMPL(ImplType, ImplName)
    #define LM_COMPONENT_REGISTER_IMPL_DEFAULT(ImplType)
#else
    #define LM_COMPONENT_REGISTER_IMPL(ImplType, ImplName) \
	    namespace { \
		    template <typename T> \
		    class ImplEntry_Init; \
		    template <> \
            class ImplEntry_Init<ImplType> { static const ImplEntry<ImplType>& reg; }; \
            const ImplEntry<ImplType>& ImplEntry_Init<ImplType>::reg = ImplEntry<ImplType>::Instance(ImplName); \
        }
    #define LM_COMPONENT_REGISTER_IMPL_DEFAULT(ImplType) \
        LM_COMPONENT_REGISTER_IMPL(ImplType, ImplType::Type_().name)
#endif

#pragma endregion

LM_NAMESPACE_END
