#
# Find ctemplate
#
# Try to find ctemplate.
# This module defines the following variables:
# - CTEMPLATE_INCLUDE_DIRS
# - CTEMPLATE_LIBRARIES
# - CTEMPLATE_FOUND
#
# The following variables can be set as arguments for the module.
# - CTEMPLATE_ROOT_DIR : Root library directory of ctemplate
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
		CTEMPLATE_INCLUDE_DIR
		NAMES ctemplate/template.h
		PATHS
			$ENV{PROGRAMFILES}/include
			${CTEMPLATE_ROOT_DIR}/include
		DOC "The directory where ctemplate/template.h resides")

	# Find library files
	find_library(
		CTEMPLATE_LIBRARY_RELEASE
		NAMES libctemplate
		PATHS
			$ENV{PROGRAMFILES}/lib
			${CTEMPLATE_ROOT_DIR}/lib)

	find_library(
		CTEMPLATE_LIBRARY_DEBUG
		NAMES libctemplated
		PATHS
			$ENV{PROGRAMFILES}/lib
			${CTEMPLATE_ROOT_DIR}/lib)
else()
	# Find include files
	find_path(
		CTEMPLATE_INCLUDE_DIR
		NAMES ctemplate/template.h
		PATHS
			/usr/include
			/usr/local/include
			/sw/include
			/opt/local/include
		DOC "The directory where ctemplate/template.h resides")

	# Find library files
	find_library(
		CTEMPLATE_LIBRARY
		NAMES ctemplate
		PATHS
			/usr/lib64
			/usr/lib
			/usr/local/lib64
			/usr/local/lib
			/sw/lib
			/opt/local/lib
			${CTEMPLATE_ROOT_DIR}/lib
		DOC "The ctemplate library")
endif()

if (WIN32)
	# Handle REQUIRD argument, define *_FOUND variable
	find_package_handle_standard_args(CTemplate DEFAULT_MSG CTEMPLATE_INCLUDE_DIR CTEMPLATE_LIBRARY_DEBUG CTEMPLATE_LIBRARY_RELEASE)

	# Define CTEMPLATE_LIBRARIES and CTEMPLATE_INCLUDE_DIRS
	if (CTEMPLATE_FOUND)
		set(CTEMPLATE_LIBRARIES debug ${CTEMPLATE_LIBRARY_DEBUG} optimized ${CTEMPLATE_LIBRARY_RELEASE})
		set(CTEMPLATE_INCLUDE_DIRS ${CTEMPLATE_INCLUDE_DIR})
	endif()

	# Hide some variables
	mark_as_advanced(CTEMPLATE_INCLUDE_DIR CTEMPLATE_LIBRARY)
else()
	# Handle REQUIRD argument, define *_FOUND variable
	find_package_handle_standard_args(CTemplate DEFAULT_MSG CTEMPLATE_INCLUDE_DIR CTEMPLATE_LIBRARY)

	# Define CTEMPLATE_LIBRARIES and CTEMPLATE_INCLUDE_DIRS
	if (CTEMPLATE_FOUND)
		set(CTEMPLATE_LIBRARIES ${CTEMPLATE_LIBRARY})
		set(CTEMPLATE_INCLUDE_DIRS ${CTEMPLATE_INCLUDE_DIR})
	endif()

	# Hide some variables
	mark_as_advanced(CTEMPLATE_INCLUDE_DIR CTEMPLATE_LIBRARY)
endif()