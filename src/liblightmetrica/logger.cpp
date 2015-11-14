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
#include <lightmetrica/logger.h>

#include <boost/bind.hpp>
#pragma warning(push)
#pragma warning(disable:4267)
#pragma warning(disable:4005)
#include <boost/asio.hpp>
#pragma warning(pop)

#include <tbb/tbb.h>

#if LM_PLATFORM_WINDOWS
#include <Windows.h>
#endif

LM_NAMESPACE_BEGIN

/*
    LoggerImpl

    Implementation of Logger class.

    TODO
    - Changiable verbose level.
    - Output-to-file mode.
    - Connection to signal
        + Portable signal-slot system is another topic.
*/
class LoggerImpl
{
public:

	static LoggerImpl* Instance();

public:

	auto Run() -> void
	{
		ioThread = std::thread(boost::bind(&boost::asio::io_service::run, &io));
	}

	auto Stop() -> void
	{
		if (ioThread.joinable())
		{
            // Clear the `work` associated to `io_service`
            // and wait for io thread ends.
			work.reset();
			ioThread.join();

            // Reset io_service for later invocation of the `run` function
            // Also we need to reassign a work.
            io.reset();
            work.reset(new boost::asio::io_service::work(io));
		}
	}

public:

	auto Log(LogType type, const std::string& message, int line, bool inplace) -> void
	{
		int threadId;
		{
			const auto id = boost::lexical_cast<std::string>(std::this_thread::get_id());
			tbb::concurrent_hash_map<std::string, int>::accessor a;
			if (threadIdMap.insert(a, id))
			{
                a->second = (int)(threadIdMap.size() - 1);
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
				#if LM_PLATFORM_WINDOWS
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
				#elif LM_PLATFORM_LINUX
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

    auto UpdateIndentation(bool push) -> void
    {
        io.post([this, push]()
        {
            Indentation += push ? 1 : -1;
            if (Indentation > 0)
            {
                IndentationString = std::string(4 * Indentation, '.') + " ";
            }
            else
            {
                Indentation = 0;
                IndentationString = "";
            }
        });
    }

private:

	auto GenerateMessage(LogType type, const std::string& message, int line, int threadId) const -> std::string
	{
		const std::string LogTypeString[] = { "ERROR", "WARN", "INFO", "DEBUG" };
		const auto now = std::chrono::high_resolution_clock::now();
		const double elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - LogStartTime).count() / 1000.0;
		return boost::str(boost::format("| %-5s %.3f | @%4d | #%2d | %s%s") % LogTypeString[(int)(type)] % elapsed % line % threadId % IndentationString % message);
	}

	auto BeginTextColor(LogType type) -> void
	{
		#if LM_PLATFORM_WINDOWS
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

	auto EndTextColor() -> void
	{
		#if LM_PLATFORM_WINDOWS
		HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
		SetConsoleTextAttribute(consoleHandle, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
		#elif LM_PLATFORM_LINUX
		std::cout << "\033[0m";
		#endif
	}

private:

	static std::atomic<LoggerImpl*> instance;
	static std::mutex mutex;

private:

	boost::asio::io_service io;
	std::unique_ptr<boost::asio::io_service::work> work{new boost::asio::io_service::work(io)};
	std::thread ioThread;

private:

	std::chrono::high_resolution_clock::time_point LogStartTime = std::chrono::high_resolution_clock::now();
	int Indentation = 0;
	std::string IndentationString;
	bool prevMessageIsInplace = false;
	tbb::concurrent_hash_map<std::string, int> threadIdMap;

};

std::atomic<LoggerImpl*> LoggerImpl::instance;
std::mutex LoggerImpl::mutex;

auto LoggerImpl::Instance() -> LoggerImpl*
{
	auto* p = instance.load(std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_acquire);
	if (p == nullptr)
	{
		std::lock_guard<std::mutex> lock(mutex);
		p = instance.load(std::memory_order_relaxed);
		if (p == nullptr)
		{
			p = new LoggerImpl;
			std::atomic_thread_fence(std::memory_order_release);
			instance.store(p, std::memory_order_relaxed);
		}
	}
	return p;
}

auto Logger_Run() -> void { LoggerImpl::Instance()->Run(); }
auto Logger_Stop() -> void { LoggerImpl::Instance()->Stop(); }
auto Logger_Log(int type, const char* message, int line, bool inplace) -> void { LoggerImpl::Instance()->Log((LogType)(type), message, line, inplace); }
auto Logger_UpdateIndentation(bool push) -> void { LoggerImpl::Instance()->UpdateIndentation(push); }

LM_NAMESPACE_END
