#pragma once

#include <stdint.h>

#include <thread>
#include <string>

#include <functional>

using NetMsgCallback = std::function<void(const uint8_t *, int)>;

class NetServer
{
public:
    NetServer();
    ~NetServer();

    bool Init(const std::string& ip, int port, bool tcp = false);

    bool Start();

    void Stop();

    void Cleanup();

    bool IsRunning() { return isRunning_; }

    void RegisterCallback(NetMsgCallback f) { cb_ = f; }

private:
    int fd_;

    bool isRunning_;
    bool isTcp_;

    NetMsgCallback cb_;

    std::thread readThread_;

    void UnInit();

    void ReadThreadFunc();
};

