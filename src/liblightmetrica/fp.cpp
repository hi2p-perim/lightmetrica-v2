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
#include <lightmetrica/fp.h>
#include <lightmetrica/logger.h>

#if LM_PLATFORM_WINDOWS
#include <Windows.h>
#endif

LM_NAMESPACE_BEGIN

#if LM_PLATFORM_WINDOWS

class FPUtilsImpl
{
public:

    static FPUtilsImpl* Instance()
    {
        thread_local FPUtilsImpl instance;
        return &instance;
    }

private:

    bool enabled_ = false;      // Current state of the floating point exception {enabled, disabled}
    std::deque<bool> stateQ_;   // State queue

public:

    auto EnableFPControl() -> void
    {
        _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
        SetFPException((unsigned int)(~(_EM_INVALID | _EM_ZERODIVIDE)));
        enabled_ = true;
    }

    auto DisableFPControl() -> void
    {
        _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_OFF);
        SetFPException((unsigned int)(_EM_INVALID | _EM_DENORMAL | _EM_ZERODIVIDE | _EM_OVERFLOW | _EM_UNDERFLOW | _EM_INEXACT));
        enabled_ = false;
    }

    auto PushFPControl() -> void
    {
        stateQ_.push_back(enabled_);
    }

    auto PopFPControl() -> void
    {
        if (stateQ_.empty())
        {
            LM_LOG_WARN("Failed to pop floating point exception state");
            return;
        }
        const bool prev = stateQ_.back(); stateQ_.pop_back();
        if (prev)
        {
            EnableFPControl();
        }
        else
        {
            DisableFPControl();
        }
    }

private:

    auto SetFPException(unsigned int newFPState) -> bool
    {
        errno_t error;

        // Get current floating-point control word
        unsigned int currentFPState;
        if ((error = _controlfp_s(&currentFPState, 0, 0)) != 0)
        {
            LM_LOG_ERROR("_controlfp_s failed : " + std::string(strerror(error)));
            return false;
        }

        // Set a new control word
        if ((error = _controlfp_s(&currentFPState, newFPState, _MCW_EM)) != 0)
        {
            LM_LOG_ERROR("_controlfp_s failed : " + std::string(strerror(error)));
            return false;
        }

        return true;
    }

};

#endif

// --------------------------------------------------------------------------------

  
auto FPUtils_EnableFPControl() -> void
{
    #if LM_PLATFORM_WINDOWS
    FPUtilsImpl::Instance()->EnableFPControl();
    #endif
}

auto FPUtils_DisableFPControl() -> void
{
    #if LM_PLATFORM_WINDOWS
    FPUtilsImpl::Instance()->DisableFPControl();
    #endif
}

auto FPUtils_PushFPControl() -> void
{
    #if LM_PLATFORM_WINDOWS
    FPUtilsImpl::Instance()->PushFPControl();
    #endif
}

auto FPUtils_PopFPControl() -> void
{
    #if LM_PLATFORM_WINDOWS
    FPUtilsImpl::Instance()->PopFPControl();
    #endif
}

LM_NAMESPACE_END
