/*
    Lightmetrica - A modern, research-oriented renderer

    Copyright (c) 2015 Hisanari Otsu

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.
*/

#pragma once

#include <lightmetrica/macros.h>
#include <string>

LM_NAMESPACE_BEGIN

/*!
	\brief Version information.

	The class is used to get version information of the library.
*/
class Version
{
public:

	LM_DISABLE_CONSTRUCT(Version);

public:

	/*!
		\brief Get the major version of the library.

		\return Major version.
	*/
    LM_PUBLIC_API static auto Major() -> std::string;

	/*!
		\brief Get the minor version of the library.

		\return Minor version.
	*/
    LM_PUBLIC_API static auto Minor() -> std::string;

	/*!
		\brief Get the patch version of the library.

		\return Patch version.
	*/
    LM_PUBLIC_API static auto Patch() -> std::string;

	/*!
		\brief Get the revision number of the library.

		\return Revision.
	*/
    LM_PUBLIC_API static auto Revision() -> std::string;

	/*!
		\brief Get the version codename of the library.

		\return Codename.
	*/
    LM_PUBLIC_API static auto Codename() -> std::string;

	/*!
		\brief Get the build date of the library.

		\return Build date.
	*/
    LM_PUBLIC_API static auto BuildDate() -> std::string;
	
	/*!
		\brief Get the formatted version of the library.

		Returns the formatted version in \a major.minor.patch.revision.
		
        \return Formatted version.
	*/
    LM_PUBLIC_API static auto Formatted() -> std::string;

	/*!
		\brief Get the platform name.

		\return Platform name.
	*/
    LM_PUBLIC_API static auto Platform() -> std::string;

	/*!
		\brief Get the architecture name.

		\return Architecture name.
	*/
    LM_PUBLIC_API static auto Archtecture() -> std::string;

};

LM_NAMESPACE_END
