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

# Module path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Build options
include(LMBuildOptions)

# External dependencies
include(LMExternalDependencies)

# Platform-specific options
include(LMPlatformOptions)

# Version
include(LMVersion)

# --------------------------------------------------------------------------------

# Include directory for generated headers
include_directories(${PROJECT_BINARY_DIR})

# Include directory for project specific libraries
include_directories("include")

# Include directory for precompiled headers
include_directories("pch")

# Output directories 
set(_OUTPUT_ROOT "${CMAKE_SOURCE_DIR}/${LM_DIST_DIR_NAME}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${_OUTPUT_ROOT}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${_OUTPUT_ROOT}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${_OUTPUT_ROOT}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG "${_OUTPUT_ROOT}/lib/Debug")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${_OUTPUT_ROOT}/lib/Debug")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${_OUTPUT_ROOT}/bin/Debug")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE "${_OUTPUT_ROOT}/lib/Release")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${_OUTPUT_ROOT}/lib/Release")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${_OUTPUT_ROOT}/bin/Release")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${_OUTPUT_ROOT}/lib/RelWithDebInfo")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO "${_OUTPUT_ROOT}/lib/RelWithDebInfo")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${_OUTPUT_ROOT}/bin/RelWithDebInfo")

# Specify to use solution directories
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# --------------------------------------------------------------------------------

# Add projects
add_subdirectory("src/liblightmetrica")
add_subdirectory("src/lightmetrica")
add_subdirectory("src/lightmetrica-test")

# Plugin
add_subdirectory("plugin")

# --------------------------------------------------------------------------------

# Packaging
include(LMPackaging)
