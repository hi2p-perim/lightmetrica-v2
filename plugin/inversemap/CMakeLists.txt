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

include(AddPlugin)
set(_PROJECT_NAME "inversemap")
add_plugin(
	NAME ${_PROJECT_NAME}
	SOURCE
		"inversemaputils.h"
		"mltutils.h"
        "renderer_invmap_pssmltfixed.cpp"
        "renderer_invmap_mltfixed.cpp"
        "renderer_invmap_mltinvmapfixed.cpp"
        "renderer_invmap_ptfixed.cpp"
		"renderer_invmap_bdptfixed.cpp"
		"renderer_invmap_pssmlt.cpp"
		"renderer_invmap_mmltfixed.cpp"
		"renderer_invmap_mmltinvmapfixed.cpp"
	NO_INSTALL)
target_link_libraries(${_PROJECT_NAME} liblightmetrica ${Boost_LIBRARIES})
add_dependencies(${_PROJECT_NAME} liblightmetrica)


# Test
include (PCHTargets)
set(_PROJECT_NAME "inversemap-test")
set(_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/include/lightmetrica-test")
set(_GTEST_SOURCE_DIR "${PROJECT_SOURCE_DIR}/external-src/gtest-1.7.0")
pch_add_executable(
    ${_PROJECT_NAME}
    PCH_HEADER "${PROJECT_SOURCE_DIR}/pch/pch_test.h"
    "${_INCLUDE_DIR}/macros.h"
    "${_INCLUDE_DIR}/utils.h"
    "${_INCLUDE_DIR}/mathutils.h"
    "${PROJECT_SOURCE_DIR}/src/lightmetrica-test/main.cpp"
    "inversemap_test.cpp"
    "${_GTEST_SOURCE_DIR}/src/gtest-all.cc")
target_link_libraries(${_PROJECT_NAME} ${COMMON_LIBRARIES} liblightmetrica)
add_dependencies(${_PROJECT_NAME} liblightmetrica)
set_target_properties(${_PROJECT_NAME} PROPERTIES FOLDER "test")
