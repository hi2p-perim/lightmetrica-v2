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
#include <lightmetrica/debug.h>
#include <lightmetrica/logger.h>

#if LM_PLATFORM_WINDOWS
#include <Windows.h>
#pragma warning(push)
#pragma warning(disable:4091)
#include <DbgHelp.h>
#pragma warning(pop)
#pragma comment(lib, "Dbghelp.lib")
#endif

LM_NAMESPACE_BEGIN

// https://msdn.microsoft.com/library/bb204633(v=vs.85).aspx
// http://stackoverflow.com/questions/590160/how-to-log-stack-frames-with-windows-x64
auto DebugUtils_StackTrace() -> bool
{
    #if !LM_PLATFORM_WINDOWS

    LM_LOG_WARN("Stack trace is unsupported");
    return false;

    #else

    typedef USHORT(WINAPI *CaptureStackBackTraceType)(__in ULONG, __in ULONG, __out PVOID*, __out_opt PULONG);
    CaptureStackBackTraceType func = (CaptureStackBackTraceType)(GetProcAddress(LoadLibrary("kernel32.dll"), "RtlCaptureStackBackTrace"));
    if (func == NULL)
    {
        return false;
    }

    const int kMaxCallers = 62;

    void* callers_stack[kMaxCallers];
    unsigned short frames;
    SYMBOL_INFO* symbol;
    HANDLE process;
    process = GetCurrentProcess();
    SymInitialize(process, NULL, TRUE);
    frames = (func)(0, kMaxCallers, callers_stack, NULL);
    symbol = (SYMBOL_INFO *)calloc(sizeof(SYMBOL_INFO) + 256 * sizeof(char), 1);
    symbol->MaxNameLen = 255;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

    const unsigned short MAX_CALLERS_SHOWN = 10;
    frames = frames < MAX_CALLERS_SHOWN ? frames : MAX_CALLERS_SHOWN;

    std::stringstream ss;
    for (unsigned int i = 0; i < frames; i++)
    {
        SymFromAddr(process, (DWORD64)(callers_stack[i]), 0, symbol);
        ss << i << ": " << callers_stack[i] << " " << symbol->Name << " - 0x" << symbol->Address << std::endl;
    }

    LM_LOG_ERROR(ss.str());

    free(symbol);
    return true;

    #endif
}

LM_NAMESPACE_END
