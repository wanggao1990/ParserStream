#pragma once

#include <stdint.h>

#include <thread>
#include <string>

#include <functional>

using UdpMsgCallback = std::function<void(const uint8_t *, int)>;

class UdpServer
{
public:
    UdpServer();
    ~UdpServer();

    bool Init(const std::string& ip, int port);

    bool Start();

    void Stop();

    void Cleanup();

    bool IsRunning() { return isRunning_; }

    void RegisterCallback(UdpMsgCallback f) { cb_ = f; }

private:
    int fd_;

    bool isRunning_;

    UdpMsgCallback cb_;

    std::thread readThread_;

    void UnInit();

    void ReadThreadFunc();
};

