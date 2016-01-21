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
#include <unordered_map>

LM_NAMESPACE_BEGIN

/*!
    Helper macros for enums.
*/

#define LM_ENUM_TYPE_MAP(EnumType)																				\
    template <typename T>																						\
    class EnumTypeMap;																							\
    template <>																									\
    class EnumTypeMap<EnumType> {																				\
    public:																										\
        EnumTypeMap() {																							\
            for (size_t i = 0; i < sizeof(EnumType##_String) / sizeof(EnumType##_String[0]); i++)				\
                TypeMap[EnumType##_String[i]] = (EnumType)(i);													\
        }																										\
        static EnumType ToEnum(const std::string& s) { return Instance().TypeMap[s]; }							\
        static EnumTypeMap<EnumType>& Instance() { static EnumTypeMap<EnumType> instance; return instance; }	\
    private:																									\
        std::unordered_map<std::string, EnumType> TypeMap;														\
    }

#define LM_ENUM_TO_STRING(EnumType, EnumValue)  EnumType##_String[(int)(EnumValue)]
#define LM_STRING_TO_ENUM(EnumType, EnumString) EnumTypeMap<EnumType>::ToEnum(EnumString)

LM_NAMESPACE_END
