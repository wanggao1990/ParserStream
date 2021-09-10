#pragma once

#include "Image.h"
#include "ImageHandler.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}

class StreamDecoder
{
public:
    StreamDecoder();
    ~StreamDecoder();

    bool Init();
    void Cleanup();

    bool GetNewImage(RGBImage & copyOfImage, int timeoutMilliSec)
    {
        return decodedImageHandler.GetNewImage(copyOfImage, timeoutMilliSec);
    }

    void DecodeBuffer(const uint8_t* buf, int len);

    bool RegisterCallback(ImageCallback f);


    RGBImageHandler decodedImageHandler;

private:

    bool initSuccess_;

    std::thread           callbackThread_;
    void                  CallbackThreadFunc();
    bool                  cbThreadIsRunning_;

    ImageCallback         cb_;

    std::mutex            mtx_;

    AVCodecContext*       pCodecCtx_;
    AVCodec*              pCodec_;
    AVCodecParserContext* pCodecParserCtx_;
    SwsContext*           pSwsCtx_;

    AVPacket*   pPacket_;
    AVFrame*    pFrameYUV_;
    AVFrame*    pFrameRGB_;
};

