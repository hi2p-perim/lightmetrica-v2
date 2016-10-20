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

#
# Project version etc.
#

# Version number and codename
# 2.0.0 - cocoa
# 2.0.1 - sansha-sanyou
# 2.0.2 - stella
set(LM_VERSION_MAJOR "2")
set(LM_VERSION_MINOR "0")
set(LM_VERSION_PATCH "1")
set(LM_VERSION_CODENAME "stella")

# Execute git command in the project root and check revision number
find_package(Git REQUIRED)
execute_process(
	COMMAND "${GIT_EXECUTABLE}" "rev-parse" "--short" "HEAD"
	OUTPUT_VARIABLE LM_VERSION_REVISION
	OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Current date
if (WIN32)
	execute_process(COMMAND "cmd" "/C date /T" OUTPUT_VARIABLE LM_CURRENT_DATE)
else()
	execute_process(COMMAND "date" "+%Y/%m/%d" OUTPUT_VARIABLE LM_CURRENT_DATE)
endif()
string(REGEX REPLACE "([0-9]+)/([0-9]+)/([0-9]+) *.*(\n|\r)$" "\\1.\\2.\\3" LM_CURRENT_DATE ${LM_CURRENT_DATE})
