#pragma once

#include "Image.h"
#include "ImageHandler.h"
//#include "StreamDecoder.h"
//#include "NetServer.h"

class NetServer;
class StreamDecoder;

#if defined _WIN32
#define STREAM_APP_API  _declspec(dllimport)
#else
#define STREAM_APP_API
#endif

class STREAM_APP_API StreamApp
{
public:
    explicit StreamApp(const std::string ip, int port, bool tcp = false);
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

    bool            tcp_;
    NetServer       *netServer_;

    StreamDecoder   *streamDecoder_;
};

