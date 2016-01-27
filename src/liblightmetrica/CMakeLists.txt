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
)
set(
    _SOURCE_FILES
)

# --------------------------------------------------------------------------------

#
# Core
#

set(
    _CORE_HEADER_FILES
    "${_INCLUDE_DIR}/macros.h"
    "${_INCLUDE_DIR}/component.h"
	"${_INCLUDE_DIR}/portable.h"
    "${_INCLUDE_DIR}/reflection.h"
	"${_INCLUDE_DIR}/logger.h"
	"${_INCLUDE_DIR}/fp.h"
	"${_INCLUDE_DIR}/static.h"
	"${_INCLUDE_DIR}/statictest.h"
	"${_INCLUDE_DIR}/exception.h"
	"${_INCLUDE_DIR}/property.h"
	"${_INCLUDE_DIR}/metacounter.h"
	"${_INCLUDE_DIR}/surfacegeometry.h"
	"${_INCLUDE_DIR}/ray.h"
	"${_INCLUDE_DIR}/intersection.h"
	"${_INCLUDE_DIR}/intersectionutils.h"
	"${_INCLUDE_DIR}/spectrum.h"
	"${_INCLUDE_DIR}/configurable.h"
	"${_INCLUDE_DIR}/enum.h"
	"${_INCLUDE_DIR}/probability.h"
	"${_INCLUDE_DIR}/debug.h"
	"${_INCLUDE_DIR}/surfaceinteraction.h"
	"${_INCLUDE_DIR}/bsdfutils.h"
	"${_INCLUDE_DIR}/scheduler.h"
	"${_INCLUDE_DIR}/renderutils.h"
	"${_INCLUDE_DIR}/align.h"
	"${_INCLUDE_DIR}/triangleutils.h"
)
set(
    _CORE_SOURCE_FILES
    "component.cpp"
    "logger.cpp"
    "fp.cpp"
	"statictest.cpp"
	"exception.cpp"
	"property.cpp"
	"debug.cpp"
	"scheduler.cpp"

    # detail
    "stringtemplate.cpp"
    "config.cpp"
    "propertyutils.cpp"
	"version.cpp"
)

source_group("${_HEADER_FILES_ROOT}\\core" FILES ${_CORE_HEADER_FILES})
source_group("${_SOURCE_FILES_ROOT}\\core" FILES ${_CORE_SOURCE_FILES})
list(APPEND _HEADER_FILES ${_CORE_HEADER_FILES})
list(APPEND _SOURCE_FILES ${_CORE_SOURCE_FILES})

# --------------------------------------------------------------------------------

#
# Core / Detail
#

set(
    _CORE_DETAIL_HEADER_FILES
	"${_INCLUDE_DIR}/detail/stringtemplate.h"
	"${_INCLUDE_DIR}/detail/config.h"
	"${_INCLUDE_DIR}/detail/propertyutils.h"
    "${_INCLUDE_DIR}/detail/version.h"
)

source_group("${_HEADER_FILES_ROOT}\\core\\detail" FILES ${_CORE_DETAIL_HEADER_FILES})
list(APPEND _HEADER_FILES ${_CORE_DETAIL_HEADER_FILES})

# --------------------------------------------------------------------------------

#
# Math
#

set(
    _MATH_HEADER_FILES
    "${_INCLUDE_DIR}/math.h"
    "${_INCLUDE_DIR}/random.h"
    "${_INCLUDE_DIR}/sampler.h"
    "${_INCLUDE_DIR}/dist.h"
)

set(_DSFMT_SOURCE_DIR "${PROJECT_SOURCE_DIR}/external/src/dSFMT-src-2.2.3")
set(
    _MATH_SOURCE_FILES
    "random.cpp"

	# dSFMT
	"${_DSFMT_SOURCE_DIR}/dSFMT.h"
	"${_DSFMT_SOURCE_DIR}/dSFMT.c"
	"${_DSFMT_SOURCE_DIR}/dSFMT-params.h"
)

source_group("${_HEADER_FILES_ROOT}\\math" FILES ${_MATH_HEADER_FILES})
source_group("${_SOURCE_FILES_ROOT}\\math" FILES ${_MATH_SOURCE_FILES})
list(APPEND _HEADER_FILES ${_MATH_HEADER_FILES})
list(APPEND _SOURCE_FILES ${_MATH_SOURCE_FILES})

# --------------------------------------------------------------------------------

#
# Asset
#

set(
    _ASSET_HEADER_FILES
	"${_INCLUDE_DIR}/asset.h"
	"${_INCLUDE_DIR}/assets.h"
	"${_INCLUDE_DIR}/primitives.h"
	"${_INCLUDE_DIR}/light.h"
	"${_INCLUDE_DIR}/emitter.h"
	"${_INCLUDE_DIR}/sensor.h"
	"${_INCLUDE_DIR}/bsdf.h"
	"${_INCLUDE_DIR}/generalizedbsdf.h"
	"${_INCLUDE_DIR}/trianglemesh.h"
	"${_INCLUDE_DIR}/film.h"
	"${_INCLUDE_DIR}/texture.h"
)

set(
    _ASSET_SOURCE_FILES
	"assets.cpp"
	"primitives.cpp"
)

source_group("${_HEADER_FILES_ROOT}\\asset" FILES ${_ASSET_HEADER_FILES})
source_group("${_SOURCE_FILES_ROOT}\\asset" FILES ${_ASSET_SOURCE_FILES})
list(APPEND _HEADER_FILES ${_ASSET_HEADER_FILES})
list(APPEND _SOURCE_FILES ${_ASSET_SOURCE_FILES})

# --------------------------------------------------------------------------------

#
# Asset / Sensor
#

set(
    _ASSET_SENSOR_SOURCE_FILES
	"asset/sensor/sensor_pinhole.cpp"
	"asset/sensor/sensor_area.cpp"
)

source_group("${_SOURCE_FILES_ROOT}\\asset\\sensor" FILES ${_ASSET_SENSOR_SOURCE_FILES})
list(APPEND _SOURCE_FILES ${_ASSET_SENSOR_SOURCE_FILES})

# --------------------------------------------------------------------------------

#
# Asset / Light
#

set(
    _ASSET_LIGHT_SOURCE_FILES
	"asset/light/light_point.cpp"
	"asset/light/light_area.cpp"
)

source_group("${_SOURCE_FILES_ROOT}\\asset\\light" FILES ${_ASSET_LIGHT_SOURCE_FILES})
list(APPEND _SOURCE_FILES ${_ASSET_LIGHT_SOURCE_FILES})

# --------------------------------------------------------------------------------

#
# Asset / Film
#

set(
	_ASSET_FILM_SOURCE_FILES
	"asset/film/film_hdr.cpp"
)

source_group("${_SOURCE_FILES_ROOT}\\asset\\film" FILES ${_ASSET_FILM_SOURCE_FILES})
list(APPEND _SOURCE_FILES ${_ASSET_FILM_SOURCE_FILES})

# --------------------------------------------------------------------------------

#
# Asset / TriangleMesh
#

set(
	_ASSET_TRIANGLE_MESH_SOURCE_FILES
	"asset/trianglemesh/trianglemesh_raw.cpp"
	"asset/trianglemesh/trianglemesh_obj.cpp"
	"asset/trianglemesh/trianglemesh_assimp.cpp"
)

source_group("${_SOURCE_FILES_ROOT}\\asset\\trianglemesh" FILES ${_ASSET_TRIANGLE_MESH_SOURCE_FILES})
list(APPEND _SOURCE_FILES ${_ASSET_TRIANGLE_MESH_SOURCE_FILES})

# --------------------------------------------------------------------------------

#
# Asset / BSDF
#

set(
	_ASSET_BSDF_SOURCE_FILES
	"asset/bsdf/bsdf_null.cpp"
	"asset/bsdf/bsdf_diffuse.cpp"
)

source_group("${_SOURCE_FILES_ROOT}\\asset\\bsdf" FILES ${_ASSET_BSDF_SOURCE_FILES})
list(APPEND _SOURCE_FILES ${_ASSET_BSDF_SOURCE_FILES})

# --------------------------------------------------------------------------------

#
# Asset / Texture
#

set(
	_ASSET_TEXTURE_SOURCE_FILES
	"asset/texture/texture_bitmap.cpp"
)

source_group("${_SOURCE_FILES_ROOT}\\asset\\texture" FILES ${_ASSET_TEXTURE_SOURCE_FILES})
list(APPEND _SOURCE_FILES ${_ASSET_TEXTURE_SOURCE_FILES})

# --------------------------------------------------------------------------------

#
# Scene
#

set(
    _SCENE_HEADER_FILES
	"${_INCLUDE_DIR}/scene.h"
	"${_INCLUDE_DIR}/accel.h"
	"${_INCLUDE_DIR}/primitive.h"
)

set(
    _SCENE_SOURCE_FILES
	"scene.cpp"
)

source_group("${_HEADER_FILES_ROOT}\\scene" FILES ${_SCENE_HEADER_FILES})
source_group("${_SOURCE_FILES_ROOT}\\scene" FILES ${_SCENE_SOURCE_FILES})
list(APPEND _HEADER_FILES ${_SCENE_HEADER_FILES})
list(APPEND _SOURCE_FILES ${_SCENE_SOURCE_FILES})

# --------------------------------------------------------------------------------

#
# Accel
#

set(
    _ACCEL_SOURCE_FILES
	"accel/accel_naive.cpp"
	"accel/accel_embree.cpp"
	"accel/accel_nanort.cpp"
)

source_group("${_SOURCE_FILES_ROOT}\\accel" FILES ${_ACCEL_SOURCE_FILES})
list(APPEND _SOURCE_FILES ${_ACCEL_SOURCE_FILES})

# --------------------------------------------------------------------------------

#
# Renderer
#

set(
    _RENDERER_HEADER_FILES
	"${_INCLUDE_DIR}/renderer.h"
)

set(
    _RENDERER_SOURCE_FILES
	"renderer/renderer_null.cpp"
	"renderer/renderer_raycast.cpp"
	"renderer/renderer_pt.cpp"
	"renderer/renderer_ptdirect.cpp"
	"renderer/renderer_lt.cpp"
	"renderer/renderer_ltdirect.cpp"
	"renderer/renderer_bdpt.cpp"
	"renderer/renderer_sampledensity.cpp"
	"renderer/renderer_sampledensity2.cpp"
	"renderer/renderer_bdpt.cpp"
)

source_group("${_HEADER_FILES_ROOT}\\renderer" FILES ${_RENDERER_HEADER_FILES})
source_group("${_SOURCE_FILES_ROOT}\\renderer" FILES ${_RENDERER_SOURCE_FILES})
list(APPEND _HEADER_FILES ${_RENDERER_HEADER_FILES})
list(APPEND _SOURCE_FILES ${_RENDERER_SOURCE_FILES})

# --------------------------------------------------------------------------------

#
# Generate header for the version number
#

set(_GENERATED_FILES_ROOT "Generated Files")
set(_GENERATED_VERSION_DEF "${PROJECT_BINARY_DIR}/versiondef.h")
configure_file(
	"versiondef.h.in"
	${_GENERATED_VERSION_DEF}
)
source_group("${_GENERATED_FILES_ROOT}" FILES ${_GENERATED_VERSION_DEF})
list(APPEND _SOURCE_FILES ${_GENERATED_VERSION_DEF})

# --------------------------------------------------------------------------------

#
# Create a library
#

pch_add_library(${_PROJECT_NAME} SHARED PCH_HEADER "${PROJECT_SOURCE_DIR}/pch/pch.h" ${_HEADER_FILES} ${_SOURCE_FILES})
target_link_libraries(${_PROJECT_NAME} ${Boost_LIBRARIES} ${TBB_LIBRARIES} ${YAMLCPP_LIBRARIES} ${CTEMPLATE_LIBRARIES} ${EMBREE_LIBRARIES} ${FREEIMAGE_LIBRARIES} ${ASSIMP_LIBRARIES})

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
    file(GLOB EXT_BINARY_FILES "${LM_EXTERNAL_BINARY_PATH}/*.dll")
    foreach(BINARY_FILE_NAME ${EXT_BINARY_FILES})
        add_custom_command_copy_dll(
            NAME ${_PROJECT_NAME}
            DLL ${BINARY_FILE_NAME})
    endforeach()

    # Embree
    add_custom_command_copy_dll_release_debug(
        NAME ${_PROJECT_NAME}
        DLL_RELEASE ${LM_EXTERNAL_BINARY_PATH}/Release/embree.dll
        DLL_DEBUG ${LM_EXTERNAL_BINARY_PATH}/Debug/embree.dll)

	# Assimp
    add_custom_command_copy_dll_release_debug(
        NAME ${_PROJECT_NAME}
        DLL_RELEASE ${LM_EXTERNAL_BINARY_PATH}/Release/assimp-vc140-mt.dll
        DLL_DEBUG ${LM_EXTERNAL_BINARY_PATH}/Debug/assimp-vc140-mtd.dll)

    # TBB
    get_filename_component(_TBB_DLL_PATH_RELEASE "$ENV{TBB_ROOT}/bin/$ENV{TBB_ARCH_PLATFORM}/${_GENERATOR_PREFIX}/tbb.dll" ABSOLUTE)
    get_filename_component(_TBB_DLL_PATH_DEBUG "$ENV{TBB_ROOT}/bin/$ENV{TBB_ARCH_PLATFORM}/${_GENERATOR_PREFIX}/tbb_debug.dll" ABSOLUTE)
    add_custom_command_copy_dll_release_debug(
        NAME ${_PROJECT_NAME}
        DLL_RELEASE ${_TBB_DLL_PATH_RELEASE}
        DLL_DEBUG ${_TBB_DLL_PATH_DEBUG})

endif()
