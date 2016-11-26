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

#include "debugio.h"
#include <lightmetrica/logger.h>

#include <iostream>
#include <string>
#include <memory>
#include <unordered_set>
#include <thread>
#include <mutex>
#include <functional>
#include <tuple>

#include <boost/optional.hpp>
#include <boost/signals2.hpp>
#include <boost/bind.hpp>
#pragma warning(push)
#pragma warning(disable:4267)
#pragma warning(disable:4251)
#pragma warning(disable:4005)
#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#pragma warning(pop)

LM_NAMESPACE_BEGIN

enum class CommandType
{
    SetInput,
    CheckRunning,
    GetOutput,
    Notify,
};

class Session : public std::enable_shared_from_this<Session>
{
private:

    std::string id;
    boost::asio::ip::tcp::socket socket;
    boost::signals2::signal<void(std::shared_ptr<Session>)> signal_OnDisconnected;
    boost::signals2::signal<void(const std::string&)> signal_OnSetInput;
    boost::signals2::signal<int()> signal_OnCheckRunning;
    boost::signals2::signal<std::tuple<std::string, std::string>()> signal_OnGetOutput;
    boost::signals2::signal<void()> signal_OnNotify;

public:

    Session(const std::string& id, boost::asio::ip::tcp::socket socket)
        : id(id)
        , socket(std::move(socket))
    {
        LM_LOG_DEBUG("Connected: " + id);
    }

    auto Connect_OnDisconnected(const std::function<void(std::shared_ptr<Session>)>& func) -> boost::signals2::connection { return signal_OnDisconnected.connect(func); }
    auto Connect_OnSetInput(const std::function<void(const std::string&)>& func) -> boost::signals2::connection { return signal_OnSetInput.connect(func); }
    auto Connect_OnCheckRunning(const std::function<int()>& func) -> boost::signals2::connection { return signal_OnCheckRunning.connect(func); }
    auto Connect_OnGetOutput(const std::function<std::tuple<std::string, std::string>()>& func) -> boost::signals2::connection { return signal_OnGetOutput.connect(func); }
    auto Connect_OnNotify(const std::function<void()>& func) -> boost::signals2::connection { return signal_OnNotify.connect(func); }

public:

    auto Run() -> void
    {
        auto self = shared_from_this();
        boost::asio::spawn(socket.get_io_service(), [this, self](boost::asio::yield_context yield)
        {
            try
            {
                while (true)
                {
                    // Receive command
                    CommandType command;
                    boost::asio::async_read(socket, boost::asio::buffer(&command, sizeof(CommandType)), yield);

                    // Execute commands
                    switch (command)
                    {
                        case CommandType::SetInput:
                        {
                            //LM_LOG_DEBUG("SetInput");
                            size_t size;
                            boost::asio::async_read(socket, boost::asio::buffer(&size, sizeof(size_t)), yield);
                            char* buf = new char[size];
                            boost::asio::async_read(socket, boost::asio::buffer(buf, size), yield);
                            std::string input(buf, size);
                            LM_SAFE_DELETE_ARRAY(buf);
                            signal_OnSetInput(input);
                            break;
                        }
                        case CommandType::CheckRunning:
                        {
                            //LM_LOG_DEBUG("CheckRunning");
                            const auto running = signal_OnCheckRunning();
                            if (!running) { break; }
                            boost::asio::async_write(socket, boost::asio::buffer(&*running, sizeof(int)), yield);
                            break;
                        }
                        case CommandType::GetOutput:
                        {
                            //LM_LOG_DEBUG("GetOutput");
                            const auto output = signal_OnGetOutput();
                            if (!output) { break; }
                            const auto WriteString = [&](const std::string& s){
                                const size_t size = s.size();
                                boost::asio::async_write(socket, boost::asio::buffer(&size, sizeof(size_t)), yield);
                                boost::asio::async_write(socket, boost::asio::buffer(s.data(), s.size()), yield);
                            };
                            WriteString(std::get<0>(*output));
                            WriteString(std::get<1>(*output));
                            break;
                        }
                        case CommandType::Notify:
                        {
                            //LM_LOG_DEBUG("Notify");
                            signal_OnNotify();
                        }
                    }
                }
            }
            catch (std::exception&)
            {
                socket.close();
                LM_LOG_DEBUG("Disconnected: " + id);
                signal_OnDisconnected(self);
            }
        });
    }

};

class DebugIOImpl
{
private:

    int port_;
    boost::asio::io_service io_;
    std::unique_ptr<boost::asio::io_service::work> work_{ new boost::asio::io_service::work(io_) };
    std::thread ioThread_;

private:

    std::unordered_set<std::shared_ptr<Session>> sessions_;
    std::mutex sessionsMutex_;
    std::mutex ioMutex_;

private:

    std::string input_;
    std::tuple<std::string, std::string> output_;
    int running_ = 1;

public:

    DebugIOImpl() : port_(16117) {}
    ~DebugIOImpl() { Stop(); }

public:

    auto Run() -> void
    {
        boost::asio::spawn(io_, [this](boost::asio::yield_context yield)
        {
            boost::asio::ip::tcp::acceptor acceptor(io_, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), (unsigned short)port_));
            while (true)
            {
                boost::system::error_code ec;
                boost::asio::ip::tcp::socket socket(io_);
                acceptor.async_accept(socket, yield[ec]);
                if (ec)
                {
                    continue;
                }
                auto session = std::make_shared<Session>(std::to_string(sessions_.size()), std::move(socket));
                {
                    std::unique_lock<std::mutex> lock(sessionsMutex_);
                    sessions_.insert(session);
                }

                // --------------------------------------------------------------------------------

                session->Connect_OnDisconnected([this](std::shared_ptr<Session> disconnectedSession) -> void
                {
                    std::unique_lock<std::mutex> lock(sessionsMutex_);
                    sessions_.erase(sessions_.find(disconnectedSession));
                });
                session->Connect_OnSetInput([this](const std::string& input) -> void
                {
                    std::unique_lock<std::mutex> lock(ioMutex_);
                    input_ = input;
                });
                session->Connect_OnCheckRunning([this]() -> int
                {
                    std::unique_lock<std::mutex> lock(ioMutex_);
                    return running_;
                });
                session->Connect_OnGetOutput([this]() -> std::tuple<std::string, std::string>
                {
                    std::unique_lock<std::mutex> lock(ioMutex_);
                    return output_;
                });
                session->Connect_OnNotify([this]() -> void
                {
                    std::unique_lock<std::mutex> lock(waitMutex_);
                    waitCond_.notify_one();
                    running_ = 1;
                });

                // --------------------------------------------------------------------------------
                session->Run();
            }
        });
        ioThread_ = std::thread(boost::bind(&boost::asio::io_service::run, &io_));
    }

    auto Stop() -> void
    {
        if (ioThread_.joinable())
        {
            io_.stop();
            ioThread_.join();
        }
    }

public:

    auto Input() -> std::string
    {
        std::unique_lock<std::mutex> lock(ioMutex_);
        return input_;
    }

    auto Output(const std::string& tag, const std::string& out) -> void
    {
        std::unique_lock<std::mutex> lock(ioMutex_);
        output_ = std::make_tuple(tag, out);
    }

    auto Connected() -> bool
    {
        std::unique_lock<std::mutex> lock(sessionsMutex_);
        return !sessions_.empty();
    }

private:

    std::mutex waitMutex_;
    std::condition_variable waitCond_;

public:

    auto Wait() -> bool
    {
        // Halt current thread
        std::unique_lock<std::mutex> lock(waitMutex_);
        running_ = 0;
        waitCond_.wait(lock);   
        return true;
    }

};

DebugIO::DebugIO() : p_(new DebugIOImpl) {}
DebugIO::~DebugIO() {}
auto DebugIO::Run() -> void { p_->Run(); }
auto DebugIO::Stop() -> void { p_->Stop(); }
auto DebugIO::Input() -> std::string { return p_->Input(); }
auto DebugIO::Output(const std::string& tag, const std::string& out) -> void { p_->Output(tag, out); }
auto DebugIO::Connected() -> bool { return p_->Connected(); }
auto DebugIO::Wait() -> bool { return p_->Wait(); }

LM_NAMESPACE_END
