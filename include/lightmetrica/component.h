/*
    Lightmetrica - A modern, research-oriented renderer
    
    Copyright (c) 2015 Hisanari Otsu
    
    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files(the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions :
    
    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.
    
    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.
*/

#pragma once

#include <lightmetrica/macros.h>
#include <lightmetrica/portable.h>
#include <functional>
#include <type_traits>

LM_NAMESPACE_BEGIN

struct Component
{
    // VTable entries and user-defined data
    // User-defined data is required to hold non-portable version
    static constexpr size_t VTableNumEntries = 100;
    void* vtable_[VTableNumEntries];
    void* userdata_[VTableNumEntries];
    static constexpr int NumFuncs = 0;
};

template <int ID, typename Signature>
struct VirtualFunction;

template <int ID, typename ReturnType, typename ...ArgTypes>
struct VirtualFunction<ID, ReturnType(ArgTypes...)>
{
    using Type = ReturnType(ArgTypes...);
    Component* o;
    explicit VirtualFunction<ID, ReturnType(ArgTypes...)>(Component* o) : o(o) {};
    ReturnType operator()(ArgTypes... args) const
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
    using BaseType = Base; \
    using InterfaceType = Current; \
    static constexpr int NumFuncs = BaseType::NumFuncs + Num

// Define interface member function
#define LM_INTERFACE_F(Index, Name, Signature) \
    static constexpr int Name ## _ID_ = BaseType::NumFuncs + Index; \
    const VirtualFunction<Name ## _ID_, Signature> Name = VirtualFunctionGenerator<Name ## _ID_, Signature>::Get(this)

// --------------------------------------------------------------------------------

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

LM_NAMESPACE_END