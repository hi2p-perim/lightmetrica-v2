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

find_package(Embree)

if (EMBREE_FOUND)
    include_directories(${EMBREE_INCLUDE_DIRS})

    include(AddPlugin)

    set(_PROJECT_NAME "accel_embree")
    add_plugin(NAME ${_PROJECT_NAME} SOURCE "accel_embree.cpp")
    target_link_libraries(${_PROJECT_NAME} ${EMBREE_LIBRARIES})

    if (WIN32)
        include (CopyDLL)
        add_custom_target(copydlls_embree)
        add_custom_command_copy_dll_release_debug(
            TARGET copydlls_embree
            NAME ${_PROJECT_NAME}
            DLL_RELEASE ${LM_EXTERNAL_BINARY_PATH}/Release/embree.dll
            DLL_DEBUG ${LM_EXTERNAL_BINARY_PATH}/Debug/embree.dll)
        install(FILES "${LM_EXTERNAL_BINARY_PATH}/Release/embree.dll" DESTINATION bin)
    endif()
endif()
