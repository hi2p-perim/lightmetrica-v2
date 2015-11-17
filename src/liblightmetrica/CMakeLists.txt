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
include (CopyDLL)

# --------------------------------------------------------------------------------

set(_PROJECT_NAME "liblightmetrica")
set(_HEADER_FILES_ROOT "Header Files")
set(_SOURCE_FILES_ROOT "Source Files")
set(_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/include/lightmetrica")

# --------------------------------------------------------------------------------

#
# Header and source Files
#

set(
    _HEADER_FILES
    "${_INCLUDE_DIR}/macros.h"
    "${_INCLUDE_DIR}/component.h"
	"${_INCLUDE_DIR}/portable.h"
    "${_INCLUDE_DIR}/math.h"
    "${_INCLUDE_DIR}/version.h"
    "${_INCLUDE_DIR}/reflection.h"
	"${_INCLUDE_DIR}/logger.h"
	"${_INCLUDE_DIR}/fp.h"
	"${_INCLUDE_DIR}/static.h"
)
set(
    _SOURCE_FILES
    "component.cpp"
    "logger.cpp"
    "fp.cpp"
	"static.cpp"
)

# --------------------------------------------------------------------------------

#
# Create a library
#

pch_add_library(${_PROJECT_NAME} SHARED PCH_HEADER "${PROJECT_SOURCE_DIR}/pch/pch.h" ${_HEADER_FILES} ${_SOURCE_FILES})
target_link_libraries(${_PROJECT_NAME} ${Boost_LIBRARIES} ${TBB_LIBRARIES})

# Proprocessor definition for exporting symbols
set_target_properties(${_PROJECT_NAME} PROPERTIES COMPILE_DEFINITIONS "LM_EXPORTS")

# Remove automatic lib- prefix
set_target_properties(${_PROJECT_NAME} PROPERTIES PREFIX "")

# Solution directory
set_target_properties(${_PROJECT_NAME} PROPERTIES FOLDER "lib")

# --------------------------------------------------------------------------------

#
# Configure automatic copy of dynamic libraries
#

if(WIN32)

    # Libraries in external directory
    file(GLOB EXT_BINARY_FILES "${CMAKE_SOURCE_DIR}/external/${_GENERATOR_PREFIX}/bin/${CMAKE_LIBRARY_ARCHITECTURE}/*.dll")
    foreach(BINARY_FILE_NAME ${EXT_BINARY_FILES})
        add_custom_command_copy_dll(
            NAME ${_PROJECT_NAME}
            DLL ${BINARY_FILE_NAME})
    endforeach()

    # TBB
    get_filename_component(_TBB_DLL_PATH_RELEASE "$ENV{TBB_ROOT}/bin/$ENV{TBB_ARCH_PLATFORM}/${_GENERATOR_PREFIX}/tbb.dll" ABSOLUTE)
    get_filename_component(_TBB_DLL_PATH_DEBUG "$ENV{TBB_ROOT}/bin/$ENV{TBB_ARCH_PLATFORM}/${_GENERATOR_PREFIX}/tbb_debug.dll" ABSOLUTE)
    add_custom_command_copy_dll_release_debug(
        NAME ${_PROJECT_NAME}
        DLL_RELEASE ${_TBB_DLL_PATH_RELEASE}
        DLL_DEBUG ${_TBB_DLL_PATH_DEBUG})

endif()
