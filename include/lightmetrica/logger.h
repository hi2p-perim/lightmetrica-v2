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

#include <lightmetrica/static.h>
#include <string>

LM_NAMESPACE_BEGIN

/*!
    Log message type.

    Defines the types of log messages.
    The user wants to choose the appropriate types according to
    the severity of the log message.
    For some environment, the output message is colored
    according to the types, e.g., red for the `Error` type.
*/
enum class LogType
{
	Error,      //!< Error.
	Warn,       //!< Warning.
	Info,       //!< Information.
	Debug,      //!< Debugging (used only for the debug mode).
};


extern "C"
{
    LM_PUBLIC_API auto Logger_Run() -> void;
    LM_PUBLIC_API auto Logger_Stop() -> void;
    LM_PUBLIC_API auto Logger_SetVerboseLevel(int level) -> void;
    LM_PUBLIC_API auto Logger_Log(int type, const char* filename, const char* message, int line, bool inplace, bool simple) -> void;
    LM_PUBLIC_API auto Logger_UpdateIndentation(bool push) -> void;
    LM_PUBLIC_API auto Logger_Flush() -> void;
}

/*!
    Logger.
    
    This class is responsible for the log output of the framework.
    The output message is designed to be natural and human-readable.
    The user can query log messages with the simple set of macros.

    Before any log messages, the user implementation need to start
    the logger with `Run` function. Internally the thread
    dedicated to the log output is created and executes the event loop
    waiting for the log messages.

    Features:
    - Multi-threaded support
        + The log message can be issued from any other threads, which is
          useful for output messages within the render loop.
    - Progressive display support
        + The renderer often need to display run-time information such as
          the progress of the rendering or ETA.
          For this, we implemented a simple message rewriting mechanism
          which can work orthogonally with any other log outputs.
    - Indentation support
        + Indentation is helpful for readability of the log messages,
          e.g., the messages from the calling function can be indented.
    - Controllable verbose level

    Example:
    
    1. Log an information message
       ```
       ...
       LM_LOG_INFO("Hello, world.");
       ...
       ```
    
    2. Log a message with indentation
       ```
       ...
       {
           LM_LOG_INFO("Begin some process");
           LM_LOG_INDENTER();
           ...
       }
       ...
       ```
*/
class Logger
{
public:

    LM_DISABLE_CONSTRUCT(Logger);

public:

    static auto Run()  -> void { LM_EXPORTED_F(Logger_Run); }
    static auto Stop() -> void { LM_EXPORTED_F(Logger_Stop); }
    static auto SetVerboseLevel(int level) -> void { LM_EXPORTED_F(Logger_SetVerboseLevel, level); }
    static auto Log(LogType type, const std::string& message, const char* filename, int line, bool inplace, bool simple) -> void { LM_EXPORTED_F(Logger_Log, (int)(type), message.c_str(), filename, line, inplace, simple); }
    static auto UpdateIndentation(bool push) -> void { LM_EXPORTED_F(Logger_UpdateIndentation, push); }

    /*!
        Flush pending log messages.
    */
    static auto Flush() -> void { LM_EXPORTED_F(Logger_Flush); }

};

/*!
    Log indenter.

    Adds indentation to the log message, which is useful
    for grouping a set of log messages and generating visually pleasing log outputs. 
    This class is implicitly constructed with `LM_LOG_INDENTER` macro.
    The indenter should be used with scopes.
*/
struct LogIndenter
{
	LogIndenter()  { Logger::UpdateIndentation(true); }
	~LogIndenter() { Logger::UpdateIndentation(false); }
};

#define LM_LOG_ERROR(message)        Logger::Log(LogType::Error, message, __FILE__, __LINE__, false, false)
#define LM_LOG_WARN(message)         Logger::Log(LogType::Warn,  message, __FILE__, __LINE__, false, false)
#define LM_LOG_INFO(message)         Logger::Log(LogType::Info,  message, __FILE__, __LINE__, false, false)
#define LM_LOG_DEBUG(message)        Logger::Log(LogType::Debug, message, __FILE__, __LINE__, false, false)
#define LM_LOG_ERROR_SIMPLE(message) Logger::Log(LogType::Error, message, __FILE__, __LINE__, false, true)
#define LM_LOG_WARN_SIMPLE(message)  Logger::Log(LogType::Warn,  message, __FILE__, __LINE__, false, true)
#define LM_LOG_INFO_SIMPLE(message)  Logger::Log(LogType::Info,  message, __FILE__, __LINE__, false, true)
#define LM_LOG_DEBUG_SIMPLE(message) Logger::Log(LogType::Debug, message, __FILE__, __LINE__, false, true)
#define LM_LOG_INPLACE(message)      Logger::Log(LogType::Info,  message, __FILE__, __LINE__, true, false)
#define LM_LOG_INPLACE_END()         std::cout << std::endl
#define LM_LOG_INDENTER()            LogIndenter LM_TOKENPASTE2(logIndenter_, __LINE__)

LM_NAMESPACE_END
