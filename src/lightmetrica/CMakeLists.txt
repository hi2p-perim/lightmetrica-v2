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

set(_PROJECT_NAME "lightmetrica")

# --------------------------------------------------------------------------------

#
# Header and source Files
#

set(
    _SOURCE_FILES
    "main.cpp"
)

# --------------------------------------------------------------------------------

#
# Create an executable
#

add_executable(${_PROJECT_NAME} ${_HEADER_FILES} ${_SOURCE_FILES})
target_link_libraries(${_PROJECT_NAME} ${COMMON_LIBRARIES} ${Boost_LIBRARIES} liblightmetrica)
add_dependencies(${_PROJECT_NAME} liblightmetrica)

# Solution directory
set_target_properties(${_PROJECT_NAME} PROPERTIES FOLDER "app")

# Install
install(TARGETS ${_PROJECT_NAME} RUNTIME DESTINATION bin)