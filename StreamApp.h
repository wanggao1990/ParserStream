#pragma once

#include "Image.h"
#include "ImageHandler.h"
#include "StreamDecoder.h"
#include "UdpServer.h"

class StreamApp
{
public:
    explicit StreamApp(const std::string ip, int port);
    ~StreamApp();

    bool NewImageIsReady();

    bool GetCurrentImage(RGBImage& copyOfImage, int timeoutMillSec = 20);

    bool StartImageStream(ImageCallback cb = NULL);
    void StopImageStream();

    bool StartH264Stream(H264Callback cb = NULL);
    void StopH264Stream();

private:
    std::string     serverIp_;
    int             serverPort_;
    UdpServer       *udpServer_;

    StreamDecoder   *streamDecoder_;
};

