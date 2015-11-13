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
        + Indentation is helpful for readibilty of the log messages,
          e.g., the messages from the calling fuction can be indended.
*/
class Logger
{
public:

	static Logger* Instance();

public:

	void Run()
	{
		ioThread = std::thread(boost::bind(&boost::asio::io_service::run, &io));
	}

	void Stop()
	{
		if (ioThread.joinable())
		{
			work.reset();
			ioThread.join();
		}
	}

public:

	void Log(LogType type, const std::string& message, int line, bool inplace)
	{
		int threadId;
		{
			const auto id = boost::lexical_cast<std::string>(std::this_thread::get_id());
			tbb::concurrent_hash_map<std::string, int>::accessor a;
			if (threadIdMap.insert(a, id))
			{
				a->second = threadIdMapCount++;
			}
			threadId = a->second;
		}

		io.post([this, type, message, line, threadId, inplace]()
		{
			// Fill spaces to erase previous message
			if (prevMessageIsInplace)
			{
				int consoleWidth;
				const int DefaultConsoleWidth = 100;
				#if NGI_PLATFORM_WINDOWS
				{
					HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
					CONSOLE_SCREEN_BUFFER_INFO screenBufferInfo;
					if (!GetConsoleScreenBufferInfo(consoleHandle, &screenBufferInfo))
					{
						consoleWidth = DefaultConsoleWidth;
					}
					else
					{
						consoleWidth = screenBufferInfo.dwSize.X - 1;
					}
				}
				#elif NGI_PLATFORM_LINUX
				{
					struct winsize w;
					if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) < 0)
					{
						consoleWidth = DefaultConsoleWidth;
					}
					else
					{
						consoleWidth = w.ws_col;
					}
				}
				#endif
				std::cout << std::string(consoleWidth, ' ') << "\r";
			}

			// Print message
			BeginTextColor(type);
			const auto text = GenerateMessage(type, message, line, threadId);
			if (inplace)
			{
				std::cout << text << "\r";
				std::cout.flush();
				prevMessageIsInplace = true;
			}
			else
			{
				std::cout << text << std::endl;
				prevMessageIsInplace = false;
			}
			EndTextColor();
		});
	}

	void IncreaseIndentation() { io.post([this](){ AddIndentation(1); }); }
	void DecreaseIndentation() { io.post([this](){ AddIndentation(-1); }); }

private:

	void AddIndentation(int delta)
	{
		Indentation += delta;
		if (Indentation > 0)
		{
			IndentationString = std::string(4 * Indentation, '.') + " ";
		}
		else
		{
			Indentation = 0;
			IndentationString = "";
		}
	}

	std::string GenerateMessage(LogType type, const std::string& message, int line, int threadId) const
	{
		const std::string LogTypeString[] = { "ERROR", "WARN", "INFO", "DEBUG" };
		const auto now = std::chrono::high_resolution_clock::now();
		const double elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - LogStartTime).count() / 1000.0;
		return boost::str(boost::format("| %-5s %.3f | @%4d | #%2d | %s%s") % LogTypeString[(int)(type)] % elapsed % line % threadId % IndentationString % message);
	}

	void BeginTextColor(LogType type)
	{
		#if NGI_PLATFORM_WINDOWS
		HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
		WORD colorFlag = 0;
		switch (type)
		{
			case LogType::Error: { colorFlag = FOREGROUND_RED | FOREGROUND_INTENSITY; break; }
			case LogType::Warn:  { colorFlag = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY; break; }
			case LogType::Info:  { colorFlag = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE; break; }
			case LogType::Debug: { colorFlag = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY; break; }
		}
		SetConsoleTextAttribute(consoleHandle, colorFlag);
		#elif LM_PLATFORM_LINUX
		switch (type)
		{
			case LogType::Error: { std::cout << "\033[31m"; break; }
			case LogType::Warn:  { std::cout << "\033[33m"; break; }
			case LogType::Info:  { std::cout << "\033[37m"; break; }
			case LogType::Debug: { std::cout << "\033[137m"; break; }
		}
		#endif
	}

	void EndTextColor()
	{
		#if NGI_PLATFORM_WINDOWS
		HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
		SetConsoleTextAttribute(consoleHandle, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
		#elif NGI_PLATFORM_LINUX
		std::cout << "\033[0m";
		#endif
	}

private:

	static std::atomic<Logger*> instance;
	static std::mutex mutex;

private:

	boost::asio::io_service io;
	std::unique_ptr<boost::asio::io_service::work> work{new boost::asio::io_service::work(io)};
	std::thread ioThread;

private:

	std::string InplaceText;
	std::chrono::high_resolution_clock::time_point LogStartTime = std::chrono::high_resolution_clock::now();
	int Indentation = 0;
	std::string IndentationString;
	bool prevMessageIsInplace = false;
	tbb::concurrent_hash_map<std::string, int> threadIdMap;
	std::atomic<int> threadIdMapCount;

};

std::atomic<Logger*> Logger::instance;
std::mutex Logger::mutex;

Logger* Logger::Instance()
{
	auto* p = instance.load(std::memory_order_relaxed);
	std::atomic_thread_fence(std::memory_order_acquire);
	if (p == nullptr)
	{
		std::lock_guard<std::mutex> lock(mutex);
		p = instance.load(std::memory_order_relaxed);
		if (p == nullptr)
		{
			p = new Logger;
			std::atomic_thread_fence(std::memory_order_release);
			instance.store(p, std::memory_order_relaxed);
		}
	}
	return p;
}

class LogIndenter
{
public:
	LogIndenter()  { Logger::Instance()->IncreaseIndentation(); }
	~LogIndenter() { Logger::Instance()->DecreaseIndentation(); }
};

#define NGI_LOG_RUN()			Logger::Instance()->Run()
#define NGI_LOG_STOP()			Logger::Instance()->Stop()
#define NGI_LOG_ERROR(message)   Logger::Instance()->Log(LogType::Error, message, __LINE__, false)
#define NGI_LOG_WARN(message)    Logger::Instance()->Log(LogType::Warn,  message, __LINE__, false)
#define NGI_LOG_INFO(message)    Logger::Instance()->Log(LogType::Info,  message, __LINE__, false)
#define NGI_LOG_DEBUG(message)   Logger::Instance()->Log(LogType::Debug, message, __LINE__, false)
#define NGI_LOG_INPLACE(message) Logger::Instance()->Log(LogType::Info,  message, __LINE__, true)
#define NGI_LOG_INPLACE_END()    std::cout << std::endl
#define NGI_LOG_INDENTER()       LogIndenter _logIndenter

LM_NAMESPACE_END