#
#  Lightmetrica - A modern, research-oriented renderer
# 
#  Copyright (c) 2015 Hisanari Otsu
#  
#  Permission is hereby granted, free of charge, to any person obtaining a copy
#  of this software and associated documentation files (the "Software"), to deal
#  in the Software without restriction, including without limitation the rights
#  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
#  copies of the Software, and to permit persons to whom the Software is
#  furnished to do so, subject to the following conditions:
#  
#  The above copyright notice and this permission notice shall be included in
#  all copies or substantial portions of the Software.
#  
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
#  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
#  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
#  THE SOFTWARE.
#

cmake_minimum_required(VERSION 3.2)

project(lightmetrica)

# --------------------------------------------------------------------------------

#
# Setup some cmake-related variables.
# Configures module path, prefix, architecture, and build type.
# TODO: Support the build in ARM environment.
#

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if (MSVC)
    if (CMAKE_GENERATOR STREQUAL "Visual Studio 14 2015 Win64")
        set(_GENERATOR_PREFIX "vc14")
    else()
        message(FATAL_ERROR "Invalid generator")
    endif()
    list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/external")
    list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/external/${_GENERATOR_PREFIX}")
endif()

if (CMAKE_CL_64)
    set(CMAKE_LIBRARY_ARCHITECTURE "x64")
else()
    set(CMAKE_LIBRARY_ARCHITECTURE "i386")
endif()

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING
        "Choose build type (Debug, Release, RelWithDebInfo, or MinSizeRel)" FORCED)
endif()

# --------------------------------------------------------------------------------

#
# Build options
#

#include(CMakeDependentOption)



# --------------------------------------------------------------------------------

#
# Setup external dependencies.
# Basically all libraries should use static libraries.
#

# Boost
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
add_definitions(-DBOOST_ALL_NO_LIB)
find_package(Boost 1.59 REQUIRED COMPONENTS program_options filesystem system regex coroutine context)
include_directories(${Boost_INCLUDE_DIRS})

# Qt
# list(APPEND CMAKE_PREFIX_PATH $ENV{QTDIR})
# if (MSVC)
#     list(APPEND CMAKE_LIBRARY_PATH "C:\\Program Files (x86)\\Windows Kits\\8.0\\Lib\\win8\\um\\x64")
# endif()
# set(CMAKE_INCLUDE_CURRENT_DIR ON)
# find_package(Qt5Widgets REQUIRED)
# find_package(Qt5UiTools REQUIRED)
# find_package(Qt5Gui REQUIRED)
# find_package(Qt5Svg REQUIRED)
# set(QT_BINARY_FILES_RELEASE "icudt53" "icuin53" "icuuc53" "Qt5Core" "Qt5Gui" "Qt5Widgets" "Qt5Svg")
# set(QT_BINARY_FILES_DEBUG   "icudt53" "icuin53" "icuuc53" "Qt5Cored" "Qt5Guid" "Qt5Widgetsd" "Qt5Svgd")

# GLEW
# set(GLEW_USE_STATIC_LIBS ON)
# find_package(GLEW)
# if (GLEW_FOUND)
#     include_directories(${GLEW_INCLUDE_DIRS})
#     add_definitions(${GLEW_DEFINITIONS})
# endif()

# TBB
find_package(TBB REQUIRED)
include_directories(${TBB_INCLUDE_DIRS})

# Assimp
# find_package(Assimp REQUIRED)
# include_directories(${ASSIMP_INCLUDE_DIRS})

# FreeImage
# find_package(FreeImage REQUIRED)
# include_directories(${FREEIMAGE_INCLUDE_DIRS})

# Embree
find_package(Embree REQUIRED)
include_directories(${EMBREE_INCLUDE_DIRS})

# yaml-cpp
find_package(YamlCpp REQUIRED)
include_directories(${YAMLCPP_INCLUDE_DIRS})

# ctemplate
find_package(CTemplate REQUIRED)
add_definitions(-DCTEMPLATE_DLL_DECL=)           # Use static library
include_directories(${CTEMPLATE_INCLUDE_DIRS})

# Google test
include_directories("${PROJECT_SOURCE_DIR}/external/src/gtest-1.7.0")
include_directories("${PROJECT_SOURCE_DIR}/external/src/gtest-1.7.0/include")
# if (MSVC AND MSVC_VERSION EQUAL 1700)
#     # Workaround for VS2012
#     add_definitions(-D_VARIADIC_MAX=10)
# endif()

# --------------------------------------------------------------------------------

#
# Platform-specifc options
# Configures platform-dependent compiler options etc.
#

# MSVC
if (MSVC)
    # Ignore linker warnings in VS
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /ignore:4099")
	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /ignore:4099")
    add_definitions(-D_SCL_SECURE_NO_WARNINGS)

    # Increase stack size to 10MB
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /STACK:10000000")
	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /STACK:10000000")

    # Warning level 4, treat warning as errors
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /WX")

    # Add floating-point flag
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fp:strict")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHa")

	# Parallel build
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")

    # Platform flag (required for boost)
    # cf. http://stackoverflow.com/questions/9742003/platform-detection-in-cmake
    set(_VERSION ${CMAKE_SYSTEM_VERSION})
    string(REPLACE "." "" _VERSION ${_VERSION})
    string(REGEX REPLACE "([0-9])" "0\\1" _VERSION ${_VERSION})
    add_definitions(-D_WIN32_WINNT=0x${_VERSION})

	# SIMD support
	# TODO: Make it configurable
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /arch:AVX")
endif()

# GCC
if (CMAKE_COMPILER_IS_GNUCXX)
    # Enabling C++11 for gcc
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

    # If the build type is Debug, define macro
    if (CMAKE_BUILD_TYPE STREQUAL Debug)
        add_definitions(-D_DEBUG)
    endif()

    # Enable all warnings, treat warning as errors
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror")

    # Ignore unknown pragma
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unknown-pragmas")
endif()

# Clang
if (CMAKE_CXX_COMPILER_ID MATCHES Clang)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -stdlib=libc++")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unknown-pragmas")
endif()

# --------------------------------------------------------------------------------

#
# Include and output directories
#

# Include directory for generated headers
include_directories(${PROJECT_BINARY_DIR})

# Include directory for project specific libraries
include_directories("include")

# Include directory for precompiled headers
include_directories("pch")

# Output directories 
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# --------------------------------------------------------------------------------

#
# Project version etc.
#

# Version number and codename
set(LM_VERSION_MAJOR "2")
set(LM_VERSION_MINOR "0")
set(LM_VERSION_PATCH "0")
set(LM_VERSION_CODENAME "cocoa")

# Execute git command in the project root and check revision number
find_package(Git REQUIRED)
execute_process(
	COMMAND "${GIT_EXECUTABLE}" "rev-parse" "--short" "HEAD"
	OUTPUT_VARIABLE LM_VERSION_REVISION
	OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Current date
if (WIN32)
	execute_process(COMMAND "cmd" "/C date /T" OUTPUT_VARIABLE LM_CURRENT_DATE)
else()
	execute_process(COMMAND "date" "+%Y/%m/%d" OUTPUT_VARIABLE LM_CURRENT_DATE)
endif()
string(REGEX REPLACE "([0-9]+)/([0-9]+)/([0-9]+) *.*(\n|\r)$" "\\1.\\2.\\3" LM_CURRENT_DATE ${LM_CURRENT_DATE})

# --------------------------------------------------------------------------------

#
# Project definitions
#

# Specify to use solution directories
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Add projects
add_subdirectory("src/liblightmetrica")
#add_subdirectory("src/liblightmetrica-test")

add_subdirectory("src/lightmetrica")
add_subdirectory("src/lightmetrica-test")
