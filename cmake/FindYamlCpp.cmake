#
# Find yaml-cpp
#
# This module defines the following variables:
# - YAMLCPP_INCLUDE_DIRS
# - YAMLCPP_LIBRARIES
# - YAMLCPP_FOUND
#
# The following variables can be set as arguments for the module.
# - YAMLCPP_ROOT_DIR : Root library directory of yaml-cpp
#

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

# Additional modules
include(FindPackageHandleStandardArgs)

if (WIN32)
	# Find include files
	find_path(
		YAMLCPP_INCLUDE_DIR
		NAMES yaml-cpp/yaml.h
		PATHS
			$ENV{PROGRAMFILES}/include
			${YAMLCPP_ROOT_DIR}/include
		DOC "The directory where yaml-cpp/yaml.h resides")

	# Find library files
	find_library(
		YAMLCPP_LIBRARY_RELEASE
		NAMES libyaml-cppmd
		PATHS
			$ENV{PROGRAMFILES}/lib
			${YAMLCPP_ROOT_DIR}/lib)

	find_library(
		YAMLCPP_LIBRARY_DEBUG
		NAMES libyaml-cppmdd
		PATHS
			$ENV{PROGRAMFILES}/lib
			${YAMLCPP_ROOT_DIR}/lib)
else()
	# Find include files
	find_path(
		YAMLCPP_INCLUDE_DIR
		NAMES yaml-cpp/yaml.h
		PATHS
			/usr/include
			/usr/local/include
			/sw/include
			/opt/local/include
		DOC "The directory where yaml-cpp/yaml.h resides")

	# Find library files
	find_library(
		YAMLCPP_LIBRARY
		NAMES yaml-cpp
		PATHS
			/usr/lib64
			/usr/lib
			/usr/lib/x86_64-linux-gnu
			/usr/local/lib64
			/usr/local/lib
			/sw/lib
			/opt/local/lib
			${YAMLCPP_ROOT_DIR}/lib
		DOC "The yaml-cpp library")
endif()

if (WIN32)
	# Handle REQUIRD argument, define *_FOUND variable
	find_package_handle_standard_args(YamlCpp DEFAULT_MSG YAMLCPP_INCLUDE_DIR YAMLCPP_LIBRARY_RELEASE YAMLCPP_LIBRARY_DEBUG)

	# Define YAMLCPP_LIBRARIES and YAMLCPP_INCLUDE_DIRS
	if (YAMLCPP_FOUND)
		set(YAMLCPP_LIBRARIES_RELEASE ${YAMLCPP_LIBRARY_RELEASE})
		set(YAMLCPP_LIBRARIES_DEBUG ${YAMLCPP_LIBRARY_DEBUG})
		set(YAMLCPP_LIBRARIES debug ${YAMLCPP_LIBRARIES_DEBUG} optimized ${YAMLCPP_LIBRARY_RELEASE})
		set(YAMLCPP_INCLUDE_DIRS ${YAMLCPP_INCLUDE_DIR})
	endif()

	# Hide some variables
	mark_as_advanced(YAMLCPP_INCLUDE_DIR YAMLCPP_LIBRARY_RELEASE YAMLCPP_LIBRARY_DEBUG)
else()
	find_package_handle_standard_args(YamlCpp DEFAULT_MSG YAMLCPP_INCLUDE_DIR YAMLCPP_LIBRARY)
	
	if (YAMLCPP_FOUND)
		set(YAMLCPP_LIBRARIES ${YAMLCPP_LIBRARY})
		set(YAMLCPP_INCLUDE_DIRS ${YAMLCPP_INCLUDE_DIR})
	endif()

	mark_as_advanced(YAMLCPP_INCLUDE_DIR YAMLCPP_LIBRARY)
endif()



