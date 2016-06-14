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

# --------------------------------------------------------------------------------

#
# Install
#

# RPATH setting
# https://cmake.org/Wiki/CMake_RPATH_handling
if (APPLE)
    # Relative to the main executable
    set(CMAKE_INSTALL_RPATH "@executable_path")
endif()

# Include files
FILE(GLOB _INCLUDE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/include/lightmetrica/*.h")
install(FILES ${_INCLUDE_FILES} DESTINATION "lightmetrica/include/lightmetrica")

# Example scenes
get_filename_component(_EXAMPLE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/example" REALPATH)
install(DIRECTORY "${_EXAMPLE_PATH}/" DESTINATION "lightmetrica/example")

# --------------------------------------------------------------------------------

#
# Packaging with CPack
#

# Package version
set(CPACK_PACKAGE_VERSION       "${LM_VERSION_MAJOR}.${LM_VERSION_MINOR}.${LM_VERSION_PATCH}.${LM_VERSION_REVISION}")
set(CPACK_PACKAGE_VERSION_MAJOR ${LM_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${LM_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${LM_VERSION_PATCH})

# Package name, etc.
set(CPACK_PACKAGE_NAME "lightmetrica")
set(CPACK_PACKAGE_VENDOR "Hisanari Otsu")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A modern, research-oriented renderer")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")

# Platform specific configuration
if (WIN32)
    set(CPACK_SYSTEM_NAME "win-x86_64")
elseif(APPLE)
    set(CPACK_SYSTEM_NAME "osx-x86_64")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CPACK_SYSTEM_NAME "linux-x86_64")
endif()

# Configuration for DragNDrop installer for OSX
set(CPACK_DMG_VOLUME_NAME ${CPACK_PACKAGE_NAME})
set(CPACK_DMG_FORMAT "UDBZ")

include(CPack)
