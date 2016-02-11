#
# CopyDLL
# 
# This module provides automatic copy functions of dynamic libraries
# for Visual Studio build environment in Windows.
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

if(WIN32)

	include(CMakeParseArguments)

    #
    # add_custom_command_copy_dll
    #
    # A custom command to copy a dynamic library
    # to the same directory as a library or an executable.
    #
    # Params
    # - NAME
    #     + Library or executable name.
    #       DLL files would be copied output directory of the specified library or executable.
    # - DLL
    #     + Target DLL file
    #
    function(add_custom_command_copy_dll)
        cmake_parse_arguments(_ARG "" "TARGET;NAME;DLL" "" ${ARGN})
        add_custom_command(
            TARGET ${_ARG_TARGET}
            PRE_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${_ARG_DLL}
                    $<TARGET_FILE_DIR:${_ARG_NAME}>)
    endfunction()

	#
    # add_custom_command_copy_dll_release_debug
    #
    # A custom command to copy a dynamic library
    # to the same directory as a library or an executable.
    # If you want to separate release and debug dlls, use this function
    #
    # Params
    # - NAME
    #     + Library or executable name.
    #       DLL files would be copied output directory of the specified library or executable.
    # - DLL_RELEASE
    #     + Target DLL file (release)
    # - DLL_DEBUG
    #     + Target DLL file (debug)
    #
    function(add_custom_command_copy_dll_release_debug)
        cmake_parse_arguments(_ARG "" "TARGET;NAME;DLL_RELEASE;DLL_DEBUG" "" ${ARGN})
        add_custom_command(
            TARGET ${_ARG_TARGET}
            PRE_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<$<CONFIG:release>:${_ARG_DLL_RELEASE}>$<$<CONFIG:debug>:${_ARG_DLL_DEBUG}>"
                    "$<TARGET_FILE_DIR:${_ARG_NAME}>")
    endfunction()

endif()