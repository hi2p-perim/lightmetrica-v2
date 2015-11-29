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
    void* vtable_[VTableNumEntries];
    void* userdata_[VTableNumEntries];
    static constexpr int NumFuncs = 0;
};

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Interface definition

template <int ID, typename Signature>
struct VirtualFunction;

template <int ID, typename ReturnType, typename ...ArgTypes>
struct VirtualFunction<ID, ReturnType(ArgTypes...)>
{
    using Type = ReturnType(ArgTypes...);
    Component* o;
    explicit VirtualFunction<ID, ReturnType(ArgTypes...)>(Component* o) : o(o) {};
    auto operator()(ArgTypes... args) const -> ReturnType
    {
        // Convert argument types to portable types and call the functions stored in the vtable.
        using FuncType = ReturnType(*)(void*, Portable<ArgTypes>...);
        return reinterpret_cast<FuncType>(o->vtable_[ID])(o->userdata_[ID], Portable<ArgTypes>(args)...);
    }
};

template <int ID, typename Signature>
struct VirtualFunctionGenerator;

template <int ID, typename ReturnType, typename ...ArgTypes>
struct VirtualFunctionGenerator<ID, ReturnType(ArgTypes...)>
{
    static_assert(ID < Component::VTableNumEntries, "Excessive VTable entries");
    static auto Get(Component* o) -> VirtualFunction<ID, ReturnType(ArgTypes...)>
    {
        return VirtualFunction<ID, ReturnType(ArgTypes...)>(o);
    }
};

// Define interface class
#define LM_INTERFACE_CLASS(Current, Base, Num) \
    LM_DEFINE_CLASS_TYPE(Current, Base); \
    using BaseType = Base; \
    using InterfaceType = Current; \
    static constexpr int NumFuncs = BaseType::NumFuncs + Num

// Define interface member function
#define LM_INTERFACE_F(Index, Name, Signature) \
    static constexpr int Name ## _ID_ = BaseType::NumFuncs + Index; \
    const VirtualFunction<Name ## _ID_, Signature> Name = VirtualFunctionGenerator<Name ## _ID_, Signature>::Get(this)

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Implementation definition

template <typename Signature>
struct ImplFunctionGenerator;

template <typename ReturnType, typename ...ArgTypes>
struct ImplFunctionGenerator<ReturnType(ArgTypes...)>
{
    using PortableFunctionType = ReturnType(*)(void*, Portable<ArgTypes>...);
    static auto Get() -> PortableFunctionType
    {
        return static_cast<PortableFunctionType>(
            [](void* userdata, Portable<ArgTypes>... args) -> ReturnType
            {
                // Convert user defined implementation to original type
                using UserFunctionType = std::function<ReturnType(ArgTypes...)>;
                auto& f = *reinterpret_cast<UserFunctionType*>(userdata);
                return f((args.Get())...);
            });
    }
};

#define LM_IMPL_CLASS(Impl, Base) \
    LM_DEFINE_CLASS_TYPE(Impl, Base); \
    using ImplType = Impl; \
    using BaseType = Base

#define LM_IMPL_F(Name, Lambda) \
    struct Name ## _Init_ { \
        Name ## _Init_(ImplType* p) { \
            p->vtable_[Name ## _ID_]   = (void*)(ImplFunctionGenerator<decltype(Name)::Type>::Get()); \
            p->userdata_[Name ## _ID_] = (void*)(&p->Name ## _); \
        } \
    } Name ## _Init_Inst_{this}; \
    friend struct Name ## _Init_; \
    const std::function<decltype(Name)::Type> Name ## _ = Lambda

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
    LM_PUBLIC_API auto ComponentFactory_Register(TypeInfo implT, CreateFuncPointerType createFunc, ReleaseFuncPointerType releaseFunc) -> void;
    LM_PUBLIC_API auto ComponentFactory_Create(const char* implName)->Component*;
    LM_PUBLIC_API auto ComponentFactory_ReleaseFunc(const char* implName)->ReleaseFuncPointerType;
}

class ComponentFactory
{
public:

    LM_DISABLE_CONSTRUCT(ComponentFactory);

public:

    static auto Register(const TypeInfo& implT, CreateFuncPointerType createFunc, ReleaseFuncPointerType releaseFunc) -> void { LM_EXPORTED_F(ComponentFactory_Register, implT, createFunc, releaseFunc); }
    static auto Create(const char* implName) -> Component* { LM_EXPORTED_F(ComponentFactory_Create, implName); }
    static auto ReleaseFunc(const char* implName) -> ReleaseFuncPointerType { LM_EXPORTED_F(ComponentFactory_ReleaseFunc, implName); }

public:

    template <typename InterfaceType>
    static auto Create(const char* implName) -> std::unique_ptr<InterfaceType, ReleaseFuncPointerType>
    {
        // Create instance
        using ReturnType = std::unique_ptr<InterfaceType, ReleaseFuncPointerType>;
        auto* p = reinterpret_cast<InterfaceType*>(Create(implName));
        if (!p)
        {
            return ReturnType(nullptr, [](Component*){});
        }

        // Deleter
        const auto deleter = ReleaseFunc(implName);
        if (!deleter)
        {
            return ReturnType(nullptr, [](Component*){});
        }
        
        ReturnType p2(p, deleter);
        return std::move(p2);
    }

    template <typename InterfaceType>
    static auto Create() -> std::unique_ptr<InterfaceType, ReleaseFuncPointerType>
    {
        const auto implName = std::string(InterfaceType::Type().name) + "_";
        return std::move(Create<InterfaceType>(implName.c_str()));
    }

};

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

    static auto Instance() -> ImplEntry<ImplType>&
    {
        static ImplEntry<ImplType> instance;
        return instance;
    }

private:

    ImplEntry()
    {
        ComponentFactory::Register(
            ImplType::Type(), 
            []() -> Component* { return new ImplType; },
            [](Component* p) -> void
            {
                auto* p2 = reinterpret_cast<ImplType*>(p);
                LM_SAFE_DELETE(p2);
                p = nullptr;
            });
    }

};

#define LM_COMPONENT_REGISTER_IMPL(ImplType) \
	namespace { \
		template <typename T> \
		class ImplEntry_Init; \
		template <> \
        class ImplEntry_Init<ImplType> { static const ImplEntry<ImplType>& reg; }; \
        const ImplEntry<ImplType>& ImplEntry_Init<ImplType>::reg = ImplEntry<ImplType>::Instance(); \
	}

#pragma endregion

LM_NAMESPACE_END
