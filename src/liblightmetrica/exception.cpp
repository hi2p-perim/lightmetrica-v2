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
#include <lightmetrica/exception.h>

#if LM_PLATFORM_WINDOWS
#include <Windows.h>
#endif

LM_NAMESPACE_BEGIN

#if LM_PLATFORM_WINDOWS
namespace
{
    void SETransFunc(unsigned int code, PEXCEPTION_POINTERS data)
    {
	    std::string desc;
	    switch (code)
	    {
		    case EXCEPTION_ACCESS_VIOLATION:         { desc = "ACCESS_VIOLATION";         break; }
		    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:    { desc = "ARRAY_BOUNDS_EXCEEDED";    break; }
		    case EXCEPTION_BREAKPOINT:               { desc = "BREAKPOINT";               break; }
		    case EXCEPTION_DATATYPE_MISALIGNMENT:    { desc = "DATATYPE_MISALIGNMENT";    break; }
            case EXCEPTION_FLT_DENORMAL_OPERAND:     { desc = "FLT_DENORMAL_OPERAND";     break; }
		    case EXCEPTION_FLT_DIVIDE_BY_ZERO:       { desc = "FLT_DIVIDE_BY_ZERO";       break; }
		    case EXCEPTION_FLT_INEXACT_RESULT:       { desc = "FLT_INEXACT_RESULT";       break; }
		    case EXCEPTION_FLT_INVALID_OPERATION:    { desc = "FLT_INVALID_OPERATION";    break; }
		    case EXCEPTION_FLT_OVERFLOW:             { desc = "FLT_OVERFLOW";             break; }
		    case EXCEPTION_FLT_STACK_CHECK:          { desc = "FLT_STACK_CHECK";          break; }
		    case EXCEPTION_FLT_UNDERFLOW:            { desc = "FLT_UNDERFLOW";            break; }
		    case EXCEPTION_ILLEGAL_INSTRUCTION:      { desc = "ILLEGAL_INSTRUCTION";      break; }
		    case EXCEPTION_IN_PAGE_ERROR:            { desc = "IN_PAGE_ERROR";            break; }
		    case EXCEPTION_INT_DIVIDE_BY_ZERO:       { desc = "INT_DIVIDE_BY_ZERO";       break; }
		    case EXCEPTION_INT_OVERFLOW:             { desc = "INT_OVERFLOW";             break; }
		    case EXCEPTION_INVALID_DISPOSITION:	     { desc = "INVALID_DISPOSITION";      break; }
		    case EXCEPTION_NONCONTINUABLE_EXCEPTION: { desc = "NONCONTINUABLE_EXCEPTION"; break; }
		    case EXCEPTION_PRIV_INSTRUCTION:         { desc = "PRIV_INSTRUCTION";         break; }
		    case EXCEPTION_SINGLE_STEP:              { desc = "SINGLE_STEP";              break; }
		    case EXCEPTION_STACK_OVERFLOW:           { desc = "STACK_OVERFLOW";           break; }
	    }

        std::cerr << "Structured exception is detected" << std::endl;
        std::cerr << "    Exception code    : " << boost::str(boost::format("0x%08x") % code) << std::endl;
        std::cerr << "    Exception address : " << boost::str(boost::format("0x%08x") % data->ExceptionRecord->ExceptionAddress) << std::endl;
	    if (!desc.empty())
	    {
            std::cerr << "    Description       : " << desc << std::endl;
	    }

        throw std::runtime_error(desc);

        //std::cerr << "Aborting..." << std::endl;
        //exit(EXIT_FAILURE);
    }
}
#endif

auto SEHUtils_EnableStructuralException() -> void
{
    #if LM_PLATFORM_WINDOWS
    _set_se_translator(SETransFunc);
    #endif
}

auto SEHUtils_DisableStructuralException() -> void
{
    #if LM_PLATFORM_WINDOWS
    _set_se_translator(nullptr);
    #endif
}

LM_NAMESPACE_END
