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

include (PCHTargets)

# --------------------------------------------------------------------------------

set(_PROJECT_NAME "lightmetrica-test")
set(_HEADER_FILES_ROOT "Header Files")
set(_SOURCE_FILES_ROOT "Source Files")
set(_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/include/lightmetrica-test")
set(_GTEST_SOURCE_DIR "${PROJECT_SOURCE_DIR}/external/src/gtest-1.7.0")

# --------------------------------------------------------------------------------

#
# Header and source Files
#

set(
    _HEADER_FILES
	"${_INCLUDE_DIR}/macros.h"
	"${_INCLUDE_DIR}/utils.h"
)

set(
	_SOURCE_FILES
	"main.cpp"
	"test_component.cpp"
	"test_reflection.cpp"
	"test_logger.cpp"
	"test_math.cpp"

	"${_GTEST_SOURCE_DIR}/src/gtest-all.cc"
)

# --------------------------------------------------------------------------------

#
# Create a library project
#

pch_add_executable(${_PROJECT_NAME} PCH_HEADER "${PROJECT_SOURCE_DIR}/pch/pch_test.h" ${_HEADER_FILES} ${_SOURCE_FILES})
target_link_libraries(${_PROJECT_NAME} liblightmetrica)
add_dependencies(${_PROJECT_NAME} liblightmetrica)

# Solution directory
set_target_properties(${_PROJECT_NAME} PROPERTIES FOLDER "test")

# Add a target for coverage calculation
#if(CMAKE_COMPILER_IS_GNUCXX)
#	if(CMAKE_BUILD_TYPE STREQUAL Debug)
#		include(CodeCoverage)
#		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -fprofile-arcs -ftest-coverage")
#		setup_target_for_coverage(test.coverage lightmetrica.test test.coverage)
#	endif()
#endif()

