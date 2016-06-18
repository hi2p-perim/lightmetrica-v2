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
#include <lightmetrica/align.h>
#if LM_DEBUG_MODE
#include <lightmetrica/debug.h>
#endif

#include <functional>
#include <type_traits>
#include <memory>
#include <string>
#include <sstream>
#include <cassert>

/*!
    \defgroup component Component system
    \brief Portable component system.


    One of the major objective of this project is to create highly extensive framework
    for the renderer development. In order to achieve this goal, the user of the framework
    wants to have flexibility to extend the framework with low cost.

    The overall design of the Lightmetrica follows the design pattern known as
    the dependency injection (DI), which make is possible to decouple the dependencies between classes.
    In particular, the all extensibule classes and its functions are accessed via interfaces.
    In C++, the class with all member functions are pure virtual member functions with `=0`.
    All instantiation of interfaces are controlled via ComponentFactory factory class.

    The distinguishable feature of our system is to retain binary portability.
    That is, we can combine various plugins compiled with different compilers.
    This is benefitical because it does not restrict the users build environment.
    The users try to extend the framework can choose any compilers irrespective to the 
    compiler utilized for builing the core library and the application.
    For instance, we can even combine the core library compiled with MSVC and plugins
    compiled with Mingw.
    
    The basic idea is to implement [vtable](https://en.wikipedia.org/wiki/Virtual_method_table)
    without relying on the language features (see chapter 8 in Imperfect C++ for details).
    We offer the wrapper macros for implementing interfaces and implementations
    similar to the way we usually create virtual functions.
    Similar idea is observed in [cppcomponent](https://github.com/jbandela/cppcomponents),
    but our implementation is simpler than the existing implementation.


    ### Interface

    The interface class for our comopnent classes is the class (or struct) that
    is defined by some combination of macros.

        struct A : public Component
        {
            LM_INTERFACE_CLASS(A, Component);
            LM_INTERFACE_F(Func, void());
            LM_INTERFACE_CLASS_END();
        };

    The interface class must begin with `LM_INTERFACE_CLASS` macro
    supplying the class type (e.g., `A`) and the base class type (e.g., `Component`).
    Also the class must be finished with `LM_INTERFACE_CLASS_END` macro.

    The member functions are defined by `LM_INTERFACE_F` macro
    with the name of the function and the function signature.
    We can use some non-portable types (e.g., `std::string`, `std::vector`)
    as an argument of the function signature.
    Currently we don't support overloading and const member functions.

    Also we can create the interface with inheritances.

        struct B : public A
        {
            LM_INTERFACE_CLASS(B, A);
            LM_INTERFACE_F(Func2, void());
            LM_INTERFACE_CLASS_END();
        };


    ### Implementation

    The implementation of the interface must also be defined by the combination
    of predefined macros:
    
        struct A_Impl : public A
        {
            LM_IMPL_CLASS(A_Impl, A);
            LM_IMPL_F(Func) = [this]() -> void { ... }
        };

        LM_COMPONENT_REGISTER_IMPL(A_Impl, "a::impl");

    The class must begin with `LM_IMPL_CLASS` macro.
    The implementation of the interface functions must be defined `LM_IMPL_F` macro
    assigning the actual implemetation as a lambda function.

    Finally we need to register the implementation to the factory class
    with `LM_COMPONENT_REGISTER_IMPL` macro with a key string (in this example, `a::impl`).
    The key is later utilized as a key to create an instance of the implementation.


    ### Creating instances

    Once we create the interface class and its implementation and assumes the interface `A` is defined
    in the header `a.h`. Then we can create an instance of `A_Impl` with the factory function `ComponentFactory::Create`:

        #include "a.h"
        ...
        const auto a = ComponentFactory::Create<A>("a::impl");
        ...

    The factory function returns `unique_ptr` of the interface type.
    Note that we need careful handling of memory allocation/deallocation between boundaries
    of dynamic libraries. The function automatically registers proper deleter function
    for the instance created in the different libraries.

    \{
*/

LM_NAMESPACE_BEGIN

#pragma region Component

class Component;

//! \cond
using CreateFuncPointerType = Component* (*)();
using ReleaseFuncPointerType = void(*)(Component*);
//! \endcond

//! Base class for all component classes
class Component : public SIMDAlignedType
{
public:
    //! \cond detail

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

    // Create and release function of the instance
    CreateFuncPointerType  createFunc  = nullptr;
    ReleaseFuncPointerType releaseFunc = nullptr;

    // Number of interface functions
    static constexpr int NumInterfaces = 0;

    //! \endcond
};

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Interface definition

//! \cond detail
template <int ID, typename Iface, typename Signature>
struct VirtualFunction;

template <int ID, typename Iface, typename ReturnType, typename ...ArgTypes>
struct VirtualFunction<ID, Iface, ReturnType(ArgTypes...)>
{
	using InterfaceType = Iface;
    using Type = ReturnType(ArgTypes...);
    
    Component* o_;
    const char* name_;
    
    explicit VirtualFunction<ID, Iface, ReturnType(ArgTypes...)>(Component* o, const char* name)
        : o_(o), name_(name)
    {}

    auto Implemented() const -> bool
    {
        return o_->vt_[ID].f != nullptr;
    }

    auto operator()(ArgTypes... args) const -> ReturnType
    {
        // Convert argument types to portable types and call the functions stored in the vtable.
        // Note that return type with struct is not portable in cdecl
        // cf. http://www.angelcode.com/dev/callconv/callconv.html
        using FuncType = void(*)(void*, Portable<ReturnType>*, Portable<ArgTypes>...);
        if (!o_->vt_[ID].f)
        {
            LM_LOG_ERROR("Missing vtable entry for");
            {
                LM_LOG_INDENTER();
                LM_LOG_ERROR("Interface: " + std::string(Iface::Type_().name));
                LM_LOG_ERROR("Instance : " + std::string(o_->implName));
                LM_LOG_ERROR("Function : " + std::string(name_) + " (ID: " + std::to_string(ID) + ")");
                #if LM_DEBUG_MODE
                {
                    LM_LOG_ERROR("Stack");
                    LM_LOG_INDENTER();
                    DebugUtils::StackTrace();
                }
                #endif
            }
            LM_LOG_ERROR("Possible cause of this error:");
            {
                LM_LOG_INDENTER();
                LM_LOG_ERROR("Missing implementation. We recommend to "
                             "check if the function '" + std::string(o_->implName) + "::" + std::string(name_) + "' is properly implmeneted with RF_IMPL_F macro.");
            }
            return ReturnType();
        }
        
        #if 0
        #if LM_DEBUG_MODE
        // Print calling function
        LM_LOG_INFO("Calling interface");
        {
            LM_LOG_INDENTER();
            LM_LOG_INFO("Interface: " + std::string(Iface::Type_().name));
            LM_LOG_INFO("Instance : " + std::string(o_->implName));
            LM_LOG_INFO("Function : " + std::string(name_) + " (ID: " + std::to_string(ID) + ")");
        }
        #endif
        #endif

        Portable<ReturnType> result;
        reinterpret_cast<FuncType>(o_->vt_[ID].f)(o_->vt_[ID].implf, &result, Portable<ArgTypes>(args)...);
        return result.Get();
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
//! \endcond

/*
    \def LM_INTERFACE_CLASS(Current, Base)
    \brief Defines interface class.

    The `Base` class must be one that the `Current` class directly inherits.
    Specifying indirectly inherited classes might invoke the unexpected behavior.
    For instance, if the class is defined as
        ```
        class B : public A { ... };
        class C : public B { ... };
        ```
    the `LM_INTERFACE_CLASS` macro for the class `C` must be
        ```
        LM_INTERFACE_CLASS(C, B);
        ```
    but not
        ```
        LM_INTERFACE_CLASS(C, A);
        ```
*/
#define LM_INTERFACE_CLASS(Current, Base, N) \
    LM_DEFINE_CLASS_TYPE(Current, Base); \
    using BaseType = Base; \
    using InterfaceType = Current; \
    using UniquePtr = std::unique_ptr<InterfaceType, void(*)(Component*)>; \
    static constexpr int NumInterfaces = BaseType::NumInterfaces + N

// Define interface member function
#define LM_INTERFACE_F(ID, Name, Signature) \
        static constexpr int Name ## _ID_ = BaseType::NumInterfaces + ID; \
		using Name ## _G_ = VirtualFunctionGenerator<Name ## _ID_, InterfaceType, Signature>; \
        const VirtualFunction<Name ## _ID_, InterfaceType, Signature> Name = Name ## _G_::Get(this, #Name)

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Implementation definition

//! \cond detail
template <typename Signature>
struct ImplFunctionGenerator;

template <typename ReturnType, typename ...ArgTypes>
struct ImplFunctionGenerator<ReturnType(ArgTypes...)>
{
    using PortableFunctionType = void(*)(void*, Portable<ReturnType>*, Portable<ArgTypes>...);
    static auto Get() -> PortableFunctionType
    {
        return static_cast<PortableFunctionType>(
            [](void* userdata, Portable<ReturnType>* result, Portable<ArgTypes>... args) -> void
            {
                // Convert user defined implementation to original type
                using UserFunctionType = std::function<ReturnType(ArgTypes...)>;
                const auto& f = *reinterpret_cast<UserFunctionType*>(userdata);
                result->Set(f((args.Get())...));
            });
    }
};

template <typename ...ArgTypes>
struct ImplFunctionGenerator<void(ArgTypes...)>
{
    using PortableFunctionType = void(*)(void*, Portable<void>*, Portable<ArgTypes>...);
    static auto Get() -> PortableFunctionType
    {
        return static_cast<PortableFunctionType>(
            [](void* userdata, Portable<void>*, Portable<ArgTypes>... args) -> void
            {
                // Convert user defined implementation to original type
                using UserFunctionType = std::function<void(ArgTypes...)>;
                auto& f = *reinterpret_cast<UserFunctionType*>(userdata);
                f((args.Get())...);
            });
    }
};
//!< \endcond

#define LM_IMPL_CLASS(Impl, Base) \
    LM_DEFINE_CLASS_TYPE(Impl, Base); \
    using ImplType = Impl; \
    using BaseType = Base; \
    const struct Impl ## _Init_ { \
        Impl ## _Init_(ImplType* p) { p->implName = ImplType::Type_().name; } \
    } Impl ## _Init_Inst_{this}

#define LM_IMPL_F(Name) \
    struct Name ## _Init_ { \
        Name ## _Init_(ImplType* p) { \
            p->vt_[Name ## _ID_].f     = (void*)(ImplFunctionGenerator<decltype(BaseType::Name)::Type>::Get()); \
            p->vt_[Name ## _ID_].implf = (void*)(&p->Name ## _Impl_); \
        } \
    } Name ## _Init_Inst_{this}; \
    friend struct Name ## _Init_; \
    const std::function<decltype(BaseType::Name)::Type> Name ## _Impl_ 

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Clonable component

/*!
    \brief Clonable component

    In some case, a component instance needs to be copied to another component instances.
    From the point of the implementation, we prohibit to use copy constructors so
    all interface possible to be copied must implement this interface.
*/
class Clonable : public Component
{
public:

    LM_INTERFACE_CLASS(Clonable, Component, 1);

public:

    Clonable() = default;
    LM_DISABLE_COPY_AND_MOVE(Clonable);

public:

    /*!
        \brief Clone the instalce.

        Clones the content of the instance to the given argument.
        This function is called from `Component::Clone` function.
        The implementation of the interface must copy its data to the instance `o`.

        \param o The target instance for cloning the asset.
    */
    LM_INTERFACE_F(0, Clone, void(Clonable* o));

};

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Component factory

/*!
    Component factory.

    Instance factory class for component creation.
    All components are instantiated with this class.
*/

//! \cond detail
extern "C"
{
    LM_PUBLIC_API auto ComponentFactory_Register(const char* key, CreateFuncPointerType createFunc, ReleaseFuncPointerType releaseFunc) -> void;
    LM_PUBLIC_API auto ComponentFactory_Create(const char* key) -> Component*;
    LM_PUBLIC_API auto ComponentFactory_ReleaseFunc(const char* key) -> ReleaseFuncPointerType;
    LM_PUBLIC_API auto ComponentFactory_LoadPlugin(const char* path) -> bool;
    LM_PUBLIC_API auto ComponentFactory_LoadPlugins(const char* directory) -> void;
    LM_PUBLIC_API auto ComponentFactory_UnloadPlugins() -> void;
}
//! \endcond

class ComponentFactory
{
public:

    LM_DISABLE_CONSTRUCT(ComponentFactory);

public:

    //! \cond detail
    static auto Register(const std::string& key, CreateFuncPointerType createFunc, ReleaseFuncPointerType releaseFunc) -> void { LM_EXPORTED_F(ComponentFactory_Register, key.c_str(), createFunc, releaseFunc); }
    static auto Create(const std::string& key) -> Component* { return LM_EXPORTED_F(ComponentFactory_Create, key.c_str()); }
    static auto ReleaseFunc(const std::string& key) -> ReleaseFuncPointerType { return LM_EXPORTED_F(ComponentFactory_ReleaseFunc, key.c_str()); }
    //! \endcond

public:

    static auto LoadPlugin(const std::string& path) -> bool { return LM_EXPORTED_F(ComponentFactory_LoadPlugin, path.c_str()); }
    static auto LoadPlugins(const std::string& directory) -> void { LM_EXPORTED_F(ComponentFactory_LoadPlugins, directory.c_str()); }
    static auto UnloadPlugins() -> void { LM_EXPORTED_F(ComponentFactory_UnloadPlugins); }

public:
    
    /*!
        \brief Create an instance by the key.

        Creates an instance of the implementation given a key of 
        the implementation type specified by `LM_COMPONENT_REGISTER_IMPL` macro.
        Example:

            #include <lightmetrica/film.h>
            ...
            # Create an instance of Film_HDR
            const auto film = ComponentFactory::Create<Film>("film::hdr");
            ...
            # Now we can use the instance
            film->Splat(...);
            ...

        \tparam InterfaceType Interface type of the instance (e.g., `Film`).
        \param key Key of the implementation (e.g., `film::hdr`).
    */
    template <typename InterfaceType>
    static auto Create(const std::string& key) -> std::unique_ptr<InterfaceType, ReleaseFuncPointerType>
    {
        static_assert(std::is_base_of<Component, InterfaceType>::value, "InterfaceType must inherit Component");
        using ReturnType = std::unique_ptr<InterfaceType, ReleaseFuncPointerType>;
        auto* p = static_cast<InterfaceType*>(Create(key));
        if (!p)
        {
            LM_LOG_ERROR("Failed to create instance");
            LM_LOG_INDENTER();
            LM_LOG_ERROR("Impl: " + key);
            LM_LOG_ERROR("Interface: " + std::string(InterfaceType::Type_().name));
            return ReturnType(nullptr, nullptr);
        }
        return ReturnType(p, ReleaseFunc(key));
    }

    template <typename InterfaceType>
    static auto Create() -> std::unique_ptr<InterfaceType, ReleaseFuncPointerType>
    {
        static_assert(std::is_base_of<Component, InterfaceType>::value, "InterfaceType must inherit Component");
        const auto key = std::string(InterfaceType::Type_().name) + "_";
        return Create<InterfaceType>(key);
    }

    template <typename InterfaceType>
    static auto Clone(InterfaceType* p) -> std::unique_ptr<InterfaceType, ReleaseFuncPointerType>
    {
        static_assert(std::is_base_of<Clonable, InterfaceType>::value, "InterfaceType must inherit Clonable");
        using ReturnType = std::unique_ptr<InterfaceType, ReleaseFuncPointerType>;
        auto* p2 = static_cast<InterfaceType*>(p->createFunc());
        p2->createFunc  = p->createFunc;
        p2->releaseFunc = p->releaseFunc;
        assert(p2);
        p->Clone(p2);
        return ReturnType(p2, p->releaseFunc);
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

    static auto Instance(const std::string& key) -> ImplEntry<ImplType>&
    {
        static ImplEntry<ImplType> instance(key);
        return instance;
    }

private:

    ImplEntry(const std::string& key)
    {
        ComponentFactory::Register(
            key,
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
    #define LM_COMPONENT_REGISTER_IMPL(ImplType, Key)
    #define LM_COMPONENT_REGISTER_IMPL_DEFAULT(ImplType)
#else
    #define LM_COMPONENT_REGISTER_IMPL(ImplType, Key) \
	    namespace { \
		    template <typename T> \
		    class ImplEntry_Init; \
		    template <> \
            class ImplEntry_Init<ImplType> { static const ImplEntry<ImplType>& reg; }; \
            const ImplEntry<ImplType>& ImplEntry_Init<ImplType>::reg = ImplEntry<ImplType>::Instance(Key); \
        }
    #define LM_COMPONENT_REGISTER_IMPL_DEFAULT(ImplType) \
        LM_COMPONENT_REGISTER_IMPL(ImplType, ImplType::Type_().name)
#endif

#pragma endregion

LM_NAMESPACE_END
//! \}
