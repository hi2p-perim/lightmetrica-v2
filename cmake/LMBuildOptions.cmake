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
# Build options
#

include(CMakeDependentOption)

# LM_USE_SINGLE_PRECISION
option(LM_USE_SINGLE_PRECISION "Use single presicion floating-point number" ON)
if (LM_USE_SINGLE_PRECISION)
    add_definitions(-DLM_USE_SINGLE_PRECISION)
endif()

# LM_USE_DOUBLE_PRECISION
cmake_dependent_option(
    LM_USE_DOUBLE_PRECISION "Use double precision floating-point number" ON
    "NOT LM_USE_SINGLE_PRECISION" OFF)
if (LM_USE_DOUBLE_PRECISION)
    add_definitions(-DLM_USE_DOUBLE_PRECISION)
endif()

# Build type must be specified for make-like generators
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING
        "Choose build type (Debug, Release, RelWithDebInfo, or MinSizeRel)" FORCED)
endif()

# Distribution directory name
set(LM_DIST_DIR_NAME "dist" CACHE STRING "Distribution directory name (default: 'dist')")
