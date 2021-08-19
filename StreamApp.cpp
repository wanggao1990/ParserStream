#include "StreamApp.h"

StreamApp::StreamApp(const std::string ip, int port)
  : serverIp_(ip),
    serverPort_(port)
{
    udpServer_ = new UdpServer();
    streamDecoder_ = new StreamDecoder();
}


StreamApp::~StreamApp()
{
    streamDecoder_->RegisterCallback(NULL);
    udpServer_->RegisterCallback(NULL);

    streamDecoder_->Cleanup();
    udpServer_->Cleanup();


    if(udpServer_) {
        delete udpServer_;
    }

    if(streamDecoder_) {
        delete streamDecoder_;
    }
}

bool StreamApp::NewImageIsReady()
{
    return streamDecoder_->decodedImageHandler.NewImageIsReady();
}

bool StreamApp::GetCurrentImage(RGBImage& copyOfImage, int timeoutMillSec)
{
    return streamDecoder_->decodedImageHandler.GetNewImage(copyOfImage, timeoutMillSec);
}

bool StreamApp::StartImageStream(ImageCallback cb)
{
    if(!udpServer_->Init(serverIp_, serverPort_)) {
        printf("Initialize udp server[%s:%d] failed.\n", serverIp_.c_str(), serverPort_);
        return false;
    }

    if(!streamDecoder_->Init()) {
        printf("There is issue with decoder\n");
        return false;
    }

    udpServer_->RegisterCallback( [&](const uint8_t* buf, int len) {
        if(streamDecoder_)
            streamDecoder_->DecodeBuffer(buf, len); 
    });
    udpServer_->Start();

    if(!streamDecoder_->RegisterCallback(cb)) {
        printf("There is issue with decoder\n");
        return false;
    }

    return true;
}

void StreamApp::StopImageStream()
{
    streamDecoder_->RegisterCallback(NULL);
    udpServer_->RegisterCallback(NULL);

    streamDecoder_->Cleanup();
    udpServer_->Cleanup();
}

bool StreamApp::StartH264Stream(H264Callback cb)
{
    if(!udpServer_->Init(serverIp_, serverPort_)) {
        printf("Initialize udp server[%s:%d] failed.\n", serverIp_.c_str(), serverPort_);
        return false;
    }
    udpServer_->RegisterCallback(cb);
    udpServer_->Start();

    return true;
}

void StreamApp::StopH264Stream()
{
    streamDecoder_->RegisterCallback(NULL);
    udpServer_->RegisterCallback(NULL);

    streamDecoder_->Cleanup();
    udpServer_->Cleanup();
}
