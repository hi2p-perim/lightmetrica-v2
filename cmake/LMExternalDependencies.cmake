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

#
# Setup external dependencies.
# Basically all libraries should use static libraries.
#

# --------------------------------------------------------------------------------

if (CMAKE_CL_64)
    set(CMAKE_LIBRARY_ARCHITECTURE "x64")
else()
    set(CMAKE_LIBRARY_ARCHITECTURE "i386")
endif()

if (MSVC)
    if (CMAKE_GENERATOR STREQUAL "Visual Studio 14 2015 Win64")
        set(_GENERATOR_PREFIX "vc14")
    else()
        message(FATAL_ERROR "Invalid generator")
    endif()
    set (LM_EXTERNAL_BINARY_PATH  "${CMAKE_CURRENT_SOURCE_DIR}/external/${_GENERATOR_PREFIX}/bin/${CMAKE_LIBRARY_ARCHITECTURE}")
	set (LM_EXTERNAL_LIBRARY_PATH "${CMAKE_CURRENT_SOURCE_DIR}/external/${_GENERATOR_PREFIX}/lib/${CMAKE_LIBRARY_ARCHITECTURE}")
    list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/external")
    list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/external/${_GENERATOR_PREFIX}")
endif()

# --------------------------------------------------------------------------------

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

# OpenMP
find_package(OpenMP)
if (OpenMP_FOUND)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
endif()

# FreeImage
find_package(FreeImage REQUIRED)
include_directories(${FREEIMAGE_INCLUDE_DIRS})

# yaml-cpp
find_package(YamlCpp REQUIRED)
include_directories(${YAMLCPP_INCLUDE_DIRS})

# ctemplate
# find_package(CTemplate REQUIRED)
# add_definitions(-DCTEMPLATE_DLL_DECL=)           # Use static library
# include_directories(${CTEMPLATE_INCLUDE_DIRS})

# Google test
include_directories("${PROJECT_SOURCE_DIR}/external-src/gtest-1.7.0")
include_directories("${PROJECT_SOURCE_DIR}/external-src/gtest-1.7.0/include")
# if (MSVC AND MSVC_VERSION EQUAL 1700)
#     # Workaround for VS2012
#     add_definitions(-D_VARIADIC_MAX=10)
# endif()

# dSFMT
include_directories("${PROJECT_SOURCE_DIR}/external-src/dSFMT-src-2.2.3")
add_definitions(-DDSFMT_MEXP=19937)