#include "StreamDecoder.h"


StreamDecoder::StreamDecoder()
  : initSuccess_(false),
    cbThreadIsRunning_(false),
    cb_(nullptr),
    pCodecCtx_(nullptr),
    pCodec_(nullptr),
    pCodecParserCtx_(nullptr),
    pSwsCtx_(nullptr),
    pPacket_(nullptr),
    pFrameYUV_(nullptr),
    pFrameRGB_(nullptr)
{
}

StreamDecoder::~StreamDecoder()
{
    { // 避免死锁
        std::lock_guard<std::mutex> lck(mtx_);
        if(cb_) {
            RegisterCallback(nullptr); // thread join()
        }
    }

    Cleanup();
}

bool StreamDecoder::Init()
{
    std::lock_guard<std::mutex> lck(mtx_);

    if(true == initSuccess_) {
        printf("Decoder already initialized.\n");
        return true;
    }

    avcodec_register_all();
    pCodecCtx_ = avcodec_alloc_context3(nullptr);
    if(!pCodecCtx_) {
        return false;
    }

    pCodecCtx_->thread_count = 4;
    pCodec_ = avcodec_find_decoder(AV_CODEC_ID_H264);
    if(!pCodec_ || avcodec_open2(pCodecCtx_, pCodec_, nullptr) < 0) {
        return false;
    }

    pCodecParserCtx_ = av_parser_init(AV_CODEC_ID_H264);
    if(!pCodecParserCtx_) {
        return false;
    }

    pPacket_ = av_packet_alloc();
    if(!pPacket_) {
        return false;
    }

    pFrameYUV_ = av_frame_alloc();
    if(!pFrameYUV_) {
        return false;
    }

    pFrameRGB_ = av_frame_alloc();
    if(!pFrameRGB_) {
        return false;
    }

    pSwsCtx_ = nullptr;

    initSuccess_ = true;

    return true;
}

void StreamDecoder::Cleanup()
{
    std::lock_guard<std::mutex> lck(mtx_);

    initSuccess_ = false;

    if(nullptr != pSwsCtx_) {
        sws_freeContext(pSwsCtx_);
        pSwsCtx_ = nullptr;
    }

    if(nullptr != pCodecParserCtx_) {
        av_parser_close(pCodecParserCtx_);
        pCodecParserCtx_ = nullptr;
    }

    if(nullptr != pCodec_) {
        avcodec_close(pCodecCtx_);
        pCodec_ = nullptr;
    }

    if(nullptr != pCodecCtx_) {
        avcodec_free_context(&pCodecCtx_);
        pCodecCtx_ = nullptr;
    }

    if(nullptr != pPacket_) {
        av_packet_free(&pPacket_);
        pPacket_ = nullptr;
    }

    if(nullptr != pFrameYUV_) {
        av_frame_free(&pFrameYUV_);
        pFrameYUV_ = nullptr;
    }

    if(nullptr != pFrameRGB_) {
        av_frame_free(&pFrameRGB_);
        pFrameRGB_ = nullptr;
    }
}


//#include "opencv2\opencv.hpp"

void StreamDecoder::DecodeBuffer(const uint8_t * buf, int len)
{
    int ret;

    uint8_t* pData = const_cast<uint8_t*>(buf);
    int remainingLen = len;
    int processedLen = 0;

    static AVPixelFormat rgbPixFormat = AV_PIX_FMT_BGR24;  // 根据情况选择  
    static int rgbBufferSize;

    std::lock_guard<std::mutex> lck(mtx_);

    while(remainingLen > 0) {
        if(!pCodecParserCtx_ || !pCodecCtx_) {
            break;
        }

        processedLen = av_parser_parse2(pCodecParserCtx_, pCodecCtx_,
                                        &pPacket_->data, &pPacket_->size,
                                        pData, remainingLen,
                                        AV_NOPTS_VALUE, AV_NOPTS_VALUE, AV_NOPTS_VALUE);
        remainingLen -= processedLen;
        pData += processedLen;

        if(pPacket_->size == 0)
            continue;

        // decoded YUV frame
        ret = avcodec_send_packet(pCodecCtx_, pPacket_);
        while(ret >= 0) {
            ret = avcodec_receive_frame(pCodecCtx_, pFrameYUV_);
            if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            }
            else if(ret < 0) {
                av_log(nullptr, AV_LOG_ERROR, "Error while sending a packet to the decoder\n");
                break;
            }

            // convert YUV420P to RGB24
            if(nullptr == pSwsCtx_) {
                pSwsCtx_ = sws_getContext(pFrameYUV_->width, pFrameYUV_->height, pCodecCtx_->pix_fmt,
                                          pFrameYUV_->width, pFrameYUV_->height, rgbPixFormat,
                                          SWS_BICUBIC, nullptr, nullptr, nullptr);
            }

            if( pFrameRGB_->width != pFrameYUV_->width || pFrameRGB_->height != pFrameYUV_->height)
            {
                sws_freeContext(pSwsCtx_);
                pSwsCtx_ = sws_getContext(pFrameYUV_->width, pFrameYUV_->height, pCodecCtx_->pix_fmt,
                                          pFrameYUV_->width, pFrameYUV_->height, rgbPixFormat,
                                          SWS_BICUBIC, nullptr, nullptr, nullptr);
                
                
                av_frame_unref(pFrameRGB_);
                pFrameRGB_->width = pFrameYUV_->width;
                pFrameRGB_->height = pFrameYUV_->height;
                pFrameRGB_->format = rgbPixFormat;
                av_frame_get_buffer(pFrameRGB_, 0);    

                rgbBufferSize = pFrameRGB_->height * pFrameRGB_->width * 3; // rgb24
            }

            if(nullptr != pSwsCtx_) {
                int res = sws_scale(pSwsCtx_,(uint8_t const *const *)pFrameYUV_->data, pFrameYUV_->linesize, 0, pFrameYUV_->height,
                                    pFrameRGB_->data, pFrameRGB_->linesize);

                decodedImageHandler.WriteNewImage(pFrameRGB_->data[0], rgbBufferSize, pFrameRGB_->width, pFrameRGB_->height);

                //cv::Mat mat(h,w,CV_8UC3, pFrameRGB_->data[0]);
                //cv::imshow("mat", mat);
                //cv::waitKey(1);

                //printf("Decoded and write Image success.\n");
            }
        }
    } 

    av_packet_unref(pPacket_);
}

bool StreamDecoder::RegisterCallback(ImageCallback f)
{
    cb_  = f;

    if(cb_) {
        if(cbThreadIsRunning_) {
            printf("Callback thread already running!\n");
            return true;
        }
        cbThreadIsRunning_ = true;
        callbackThread_ = std::thread(&StreamDecoder::CallbackThreadFunc, this);
        return true;
    }
    else{
        cbThreadIsRunning_ = false;
        if(callbackThread_.joinable()) {
            callbackThread_.join();
        }
    }
}

void StreamDecoder::CallbackThreadFunc()
{
    while(cbThreadIsRunning_) {
        RGBImage copyOfImage;
        if(!decodedImageHandler.GetNewImage(copyOfImage, 1000)) {
           // printf("Decoder Callback Thread: Get image time out\n");
            continue;
        }
        
        if(cb_) {
            cb_(copyOfImage);
        }
    }
}
