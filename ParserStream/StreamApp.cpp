#include "StreamApp.h"

#include "StreamDecoder.h"
#include "NetServer.h"

StreamApp::StreamApp(const std::string ip, int port, bool tcp)
  : tcp_(tcp),
    serverIp_(ip),
    serverPort_(port)
{
    netServer_ = new NetServer();
    streamDecoder_ = new StreamDecoder();
}


StreamApp::~StreamApp()
{
    streamDecoder_->RegisterCallback(NULL);
    netServer_->RegisterCallback(NULL);

    streamDecoder_->Cleanup();
    netServer_->Cleanup();


    if(netServer_) {
        delete netServer_;
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
    if(!netServer_->Init(serverIp_, serverPort_, tcp_)) {
        printf("Initialize udp server[%s:%d] failed.\n", serverIp_.c_str(), serverPort_);
        return false;
    }

    if(!streamDecoder_->Init()) {
        printf("There is issue with decoder\n");
        return false;
    }


    //////  GDR 测试 H20，1440*1080 (接收码流开始解析前先加载一帧同分辨率全黑的SPS/PPS/IDR)
    //FILE* iframe_data = NULL;
    //fopen_s(&iframe_data, "../black_data.264", "rb");

    //char* iframe_buf = new char[8000];
    //int iframe_len = fread(iframe_buf, 1, 8000, iframe_data);
    //fclose(iframe_data);

    //streamDecoder_->DecodeBuffer((uint8_t*)iframe_buf, iframe_len);
    //free(iframe_buf);


    netServer_->RegisterCallback( [&](const uint8_t* buf, int len) {
        if(streamDecoder_)
            streamDecoder_->DecodeBuffer(buf, len); 
    });
    netServer_->Start();

    if(!streamDecoder_->RegisterCallback(cb)) {
        printf("There is issue with decoder\n");
        return false;
    }

    return true;
}

void StreamApp::StopImageStream()
{
    streamDecoder_->RegisterCallback(NULL);
    netServer_->RegisterCallback(NULL);

    streamDecoder_->Cleanup();
    netServer_->Cleanup();
}

bool StreamApp::StartH264Stream(H264Callback cb)
{
    if(!netServer_->Init(serverIp_, serverPort_, tcp_)) {
        printf("Initialize %s server[%s:%d] failed.\n", tcp_?"Tcp":"Udp", serverIp_.c_str(), serverPort_);
        return false;
    }
    netServer_->RegisterCallback(cb);
    netServer_->Start();

    return true;
}

void StreamApp::StopH264Stream()
{
    streamDecoder_->RegisterCallback(NULL);
    netServer_->RegisterCallback(NULL);

    streamDecoder_->Cleanup();
    netServer_->Cleanup();
}
