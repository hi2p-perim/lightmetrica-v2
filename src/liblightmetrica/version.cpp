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

#include <pch.h>
#include <lightmetrica/detail/version.h>
#include <versiondef.h>

LM_NAMESPACE_BEGIN

auto Version::Major()     -> std::string { return LM_VERSION_MAJOR; }
auto Version::Minor()     -> std::string { return LM_VERSION_MINOR; }
auto Version::Patch()     -> std::string { return LM_VERSION_PATCH; }
auto Version::SceneVersionMin() -> std::tuple<int, int, int>
{
    return
    {
        std::stoi(LM_SCENE_VERSION_MIN_MAJOR),
        std::stoi(LM_SCENE_VERSION_MIN_MINOR),
        std::stoi(LM_SCENE_VERSION_MIN_PATCH)
    };
}
auto Version::SceneVersionMax() -> std::tuple<int, int, int>
{
    return
    {
        std::stoi(LM_SCENE_VERSION_MAX_MAJOR),
        std::stoi(LM_SCENE_VERSION_MAX_MINOR),
        std::stoi(LM_SCENE_VERSION_MAX_PATCH)
    };
}
auto Version::Revision()  -> std::string { return LM_VERSION_REVISION; }
auto Version::BuildDate() -> std::string { return LM_BUILD_DATE; }
auto Version::Codename()  -> std::string { return LM_VERSION_CODENAME; }

auto Version::Formatted() -> std::string
{
	return boost::str(boost::format("%s.%s.%s.%s")
		% LM_VERSION_MAJOR
		% LM_VERSION_MINOR
		% LM_VERSION_PATCH
		% LM_VERSION_REVISION);
}

auto Version::Platform() -> std::string
{
#if LM_PLATFORM_WINDOWS
	return "Windows";
#elif LM_PLATFORM_LINUX
	return "Linux";
#elif LM_PLATFORM_APPLE
    return "Apple";
#endif
}

auto Version::Archtecture() -> std::string
{
#if LM_ARCH_X64
	return "x64";
#elif LM_ARCH_X86
	return "x86";
#endif
}

LM_NAMESPACE_END
