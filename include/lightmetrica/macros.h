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

// --------------------------------------------------------------------------------

#pragma region Debug mode flag

#ifndef NDEBUG
	#define LM_DEBUG_MODE 1
#else
	#define LM_DEBUG_MODE 0
#endif

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Platform flag

#ifdef _WIN32
	#define LM_PLATFORM_WINDOWS 1
#else
	#define LM_PLATFORM_WINDOWS 0
#endif

#ifdef __linux
	#define LM_PLATFORM_LINUX 1
#else
	#define LM_PLATFORM_LINUX 0
#endif

#ifdef __APPLE__
	#define LM_PLATFORM_APPLE 1
#else
	#define LM_PLATFORM_APPLE 0
#endif

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Compiler flag

#ifdef _MSC_VER
	#define LM_COMPILER_MSVC 1
#else
	#define LM_COMPILER_MSVC 0
#endif

#ifdef __INTELLISENSE__
    #define LM_INTELLISENSE 1
#else
    #define LM_INTELLISENSE 0
#endif

#if defined(__GNUC__) || defined(__MINGW32__)
	#define LM_COMPILER_GCC 1
#else
	#define LM_COMPILER_GCC 0
#endif

#ifdef __clang__
	#define LM_COMPILER_CLANG 1
	#if LM_COMPILER_GCC
		// clang defines __GNUC__
		#undef LM_COMPILER_GCC
		#define LM_COMPILER_GCC 0
	#endif
#else
	#define LM_COMPILER_CLANG 0
#endif

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Architecture flag

#if LM_COMPILER_MSVC
	#ifdef _M_IX86
		#define LM_ARCH_X86 1
	#else
		#define LM_ARCH_X86 0
	#endif
	#ifdef _M_X64
		#define LM_ARCH_X64 1
	#else
		#define LM_ARCH_X64 0
	#endif
#elif LM_COMPILER_GCC
	#ifdef __i386__
		#define LM_ARCH_X86 1
	#else
		#define LM_ARCH_X86 0
	#endif
	#ifdef __x86_64__
		#define LM_ARCH_X64 1
	#else
		#define LM_ARCH_X64 0
	#endif
#endif

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Disable some warnings

#if LM_PLATFORM_WINDOWS
	#define NOMINMAX
	#define WIN32_LEAN_AND_MEAN
	#pragma warning(disable:4819)	// Level 1. Character that cannot be represented
	#pragma warning(disable:4996)	// Level 3. _SCL_SECURE_NO_WARNINGS
	#pragma warning(disable:4290)	// Level 3. Exception specification ignored
	#pragma warning(disable:4201)	// Level 4. Nonstandard extension used : nameless struct/union
	#pragma warning(disable:4512)	// Level 4. Cannot generate an assignment operator for the given class
	#pragma warning(disable:4127)	// Level 4. Conditional expression is constant
	#pragma warning(disable:4510)	// Level 4. Default constructor could not be generated
	#pragma warning(disable:4610)	// Level 4. User-defined constructor required
	#pragma warning(disable:4100)	// Level 4. Unreferenced formal parameter
	#pragma warning(disable:4505)	// Level 4. Unreferenced local function has been removed
	#pragma warning(disable:4324)	// Level 4. Structure was padded due to __declspec(align())
	#pragma warning(disable:4702)	// Level 4. Unreachable code
    #pragma warning(disable:4189)   // Level 4. local variable is initialized but not referenced
#endif

#pragma endregion

// --------------------------------------------------------------------------------
                                  
#pragma region Dynamic library import and export

#if LM_COMPILER_MSVC
	#ifdef LM_EXPORTS
		#define LM_PUBLIC_API __declspec(dllexport)
	#else
		#define LM_PUBLIC_API __declspec(dllimport)
	#endif
	#define LM_HIDDEN_API
#elif LM_COMPILER_GCC
	#ifdef LM_EXPORTS
		#define LM_PUBLIC_API __attribute__ ((visibility("default")))
		#define LM_HIDDEN_API __attribute__ ((visibility("hidden")))
	#else
		#define LM_PUBLIC_API
		#define LM_HIDDEN_API
	#endif
#else
	#define LM_PUBLIC_API
	#define LM_HIDDEN_API
#endif

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Force inline

#if LM_COMPILER_MSVC
	#define LM_INLINE __forceinline
#elif LM_COMPILER_GCC
	#define LM_INLINE inline __attribute__((always_inline))
#endif

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Alignment

#if LM_COMPILER_MSVC
	#define LM_ALIGN(x) __declspec(align(x))
#elif LM_COMPILER_GCC
	#define LM_ALIGN(x) __attribute__((aligned(x)))
#endif
#define LM_ALIGN_16 LM_ALIGN(16)
#define LM_ALIGN_32 LM_ALIGN(32)

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Useful macros

#define LM_TOKENPASTE(x, y) x ## y
#define LM_TOKENPASTE2(x, y) LM_TOKENPASTE(x, y)
#define LM_STRINGIFY(x) #x
#define LM_STRINGIFY2(x) LM_STRINGIFY(x)
#define LM_UNUSED(x) (void)x
#define LM_UNREACHABLE() assert(0)

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

#if LM_PLATFORM_WINDOWS
	#define LM_PRAGMA(x) __pragma(x)
#else
	#define LM_PRAGMA(x) _Pragma(LM_STRINGIFY(x))
#endif

#define LM_SAFE_DELETE(val) if ((val) != nullptr) { delete (val); (val) = nullptr; }
#define LM_SAFE_DELETE_ARRAY(val) if ((val) != nullptr) { delete[] (val); (val) = nullptr; }

#define LM_DISABLE_COPY_AND_MOVE(TypeName) \
	TypeName(const TypeName &) = delete; \
	TypeName(TypeName&&) = delete; \
	auto operator=(const TypeName&) -> void = delete; \
	auto operator=(TypeName&&) -> void = delete

#define LM_DISABLE_CONSTRUCT(TypeName) \
    TypeName() = delete; \
    LM_DISABLE_COPY_AND_MOVE(TypeName)

#pragma endregion

// --------------------------------------------------------------------------------

#pragma region Framework namespace

#define LM_NAMESPACE_BEGIN namespace lightmetrica_v2 {
#define LM_NAMESPACE_END }

#pragma endregion

