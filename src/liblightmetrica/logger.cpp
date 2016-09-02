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
#if LM_COMPILER_MSVC
#pragma warning(push)
#pragma warning(disable:4267)
#pragma warning(disable:4005)
#include <boost/asio.hpp>
#pragma warning(pop)
#elif LM_COMPILER_CLANG
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-local-typedef"
#include <boost/asio.hpp>
#pragma clang diagnostic pop
#else
#include <boost/asio.hpp>
#endif

#include <tbb/tbb.h>

#if LM_PLATFORM_WINDOWS
#include <Windows.h>
#endif

LM_NAMESPACE_BEGIN

/*
    LoggerImpl

    Implementation of Logger class.

    TODO
    - Changeable verbose level.
    - Output-to-file mode.
    - Connection to signal
        + Portable signal-slot system is another topic.
*/
class LoggerImpl
{
private:

    static std::unique_ptr<LoggerImpl> instance_;

public:

    static LoggerImpl* Instance()
    {
        if (!instance_) instance_.reset(new LoggerImpl);
        return instance_.get();
    }

public:

	auto Run(bool restart = false) -> void
	{
        if (started_)
        {
            return;
        }
        started_ = true;
        if (!restart)
        {
            logStartTime_ = std::chrono::high_resolution_clock::now();
        }
		ioThread_ = std::thread(boost::bind(&boost::asio::io_service::run, &io_));
	}

	auto Stop() -> void
	{
        if (!started_)
        {
            return;
        }
		if (ioThread_.joinable())
		{
            // Clear the `work` associated to `io_service`
            // and wait for io thread ends.
			work_.reset();
			ioThread_.join();

            // Reset io_service for later invocation of the `run` function
            // Also we need to reassign a work.
            io_.reset();
            work_.reset(new boost::asio::io_service::work(io_));

            // Reset internal states
            started_ = false;
            indentation_ = 0;
            indentationString_ = "";
            prevMessageIsInplace_ = false;
            threadIdMap_.clear();
            verboseLevel_ = 0;
            unprocessedMessages_ = 0;
		}
	}

    auto SetVerboseLevel(int level) -> void
    {
        verboseLevel_ = level;
    }

	auto Log(LogType type, const std::string& message, const char* filename, int line, bool inplace, bool simple) -> void
	{
        // Ignore the `inplace` message if there are unprocessed posts
        if (inplace && unprocessedMessages_ > 0)
        {
            return;
        }

        // --------------------------------------------------------------------------------

        #pragma region Thread ID

        // The thread ID is assigned from zero with moronically increasing manner
		int threadId;
		{
			const auto id = boost::lexical_cast<std::string>(std::this_thread::get_id());
			tbb::concurrent_hash_map<std::string, int>::accessor a;
			if (threadIdMap_.insert(a, id))
			{
                a->second = (int)(threadIdMap_.size() - 1);
			}
			threadId = a->second;
		}

        #pragma endregion

        // --------------------------------------------------------------------------------

        #pragma region Post message

        // Split the message by lines and post an event to print the formatted message
        std::stringstream ss(message);
        std::string messageLine;
        while (std::getline(ss, messageLine, '\n'))
        {
            unprocessedMessages_++;
		    io_.post([this, type, messageLine, filename, line, threadId, inplace, simple]()
		    {
			    // Fill spaces to erase previous message
			    if (prevMessageIsInplace_)
			    {
				    int consoleWidth;
				    constexpr int DefaultConsoleWidth = 100;
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
				    #elif LM_PLATFORM_LINUX || LM_PLATFORM_APPLE
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
                    std::cout << std::string(consoleWidth, ' ');
                    std::cout.flush();
                    std::cout << "\r";
                    std::cout.flush();
			    }

			    // Print message
			    BeginTextColor(type);
			    const auto text = simple ? messageLine : GenerateMessage(type, messageLine, boost::filesystem::path(filename).filename().string(), line, threadId);
			    if (inplace)
			    {
                    std::cout << text;
                    std::cout.flush();
                    std::cout << "\r";
				    std::cout.flush();
				    prevMessageIsInplace_ = true;
			    }
			    else
			    {
				    std::cout << text << std::endl;
				    prevMessageIsInplace_ = false;
			    }
			    EndTextColor();

                // Mark finished
                if (unprocessedMessages_ > 0)
                {
                    unprocessedMessages_--;
                }
		    });
        }

        #pragma endregion
	}

    auto UpdateIndentation(bool push) -> void
    {
        io_.post([this, push]()
        {
            indentation_ += push ? 1 : -1;
            if (indentation_ > 0)
            {
                indentationString_ = std::string(4 * indentation_, '.') + " ";
            }
            else
            {
                indentation_ = 0;
                indentationString_ = "";
            }
        });
    }

    auto Flush() -> void
    {
        Stop();
        Run(true);
    }

private:

	auto GenerateMessage(LogType type, const std::string& message, const std::string& filename, int line, int threadId) const -> std::string
	{
		const std::string LogTypeString[] = { "ERROR", "WARN", "INFO", "DEBUG" };
		const auto now = std::chrono::high_resolution_clock::now();
		const double elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - logStartTime_).count() / 1000.0;        
        return
            verboseLevel_ == 0 ? boost::str(boost::format("| %-5s %.3f | %s%s") % LogTypeString[(int)(type)] % elapsed % indentationString_ % message) :
            verboseLevel_ == 1 ? boost::str(boost::format("| %-5s %.3f | #%2d | %s%s") % LogTypeString[(int)(type)] % elapsed % threadId % indentationString_ % message)
                               : boost::str(boost::format("| %-5s %.3f | %-8.8s~ | @%4d | #%2d | %s%s") % LogTypeString[(int)(type)] % elapsed % filename % line % threadId % indentationString_ % message);
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
		#elif LM_PLATFORM_LINUX || LM_PLATFORM_APPLE
		switch (type)
		{
			case LogType::Error: { std::cout << "\033[31m"; break; }
			case LogType::Warn:  { std::cout << "\033[33m"; break; }
			case LogType::Info:  { std::cout << "\033[00m"; break; }
			case LogType::Debug: { std::cout << "\033[37m"; break; }
		}
		#endif
	}

	auto EndTextColor() -> void
	{
		#if LM_PLATFORM_WINDOWS
		HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
		SetConsoleTextAttribute(consoleHandle, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
		#elif LM_PLATFORM_LINUX || LM_PLATFORM_APPLE
		std::cout << "\033[0m";
		#endif
	}

private:

	boost::asio::io_service io_;
	std::unique_ptr<boost::asio::io_service::work> work_{new boost::asio::io_service::work(io_)};
	std::thread ioThread_;

private:

    bool started_ = false;
    std::chrono::high_resolution_clock::time_point logStartTime_;
	int indentation_ = 0;
	std::string indentationString_;
	bool prevMessageIsInplace_ = false;
	tbb::concurrent_hash_map<std::string, int> threadIdMap_;
    int verboseLevel_ = 0;
    std::atomic<int> unprocessedMessages_{0};

};

std::unique_ptr<LoggerImpl> LoggerImpl::instance_;

auto Logger_Run() -> void { LoggerImpl::Instance()->Run(); }
auto Logger_Stop() -> void { LoggerImpl::Instance()->Stop(); }
auto Logger_SetVerboseLevel(int level) -> void { LoggerImpl::Instance()->SetVerboseLevel(level); }
auto Logger_Log(int type, const char* message, const char* filename, int line, bool inplace, bool simple) -> void { LoggerImpl::Instance()->Log((LogType)(type), message, filename, line, inplace, simple); }
auto Logger_UpdateIndentation(bool push) -> void { LoggerImpl::Instance()->UpdateIndentation(push); }
auto Logger_Flush() -> void { LoggerImpl::Instance()->Flush(); }

LM_NAMESPACE_END
