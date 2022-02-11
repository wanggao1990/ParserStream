#include "StreamDecoder.h"

#define USE_HARDWARE_DECODER


#ifdef USE_HARDWARE_DECODER

#include "H264IdrFrame.h"
class GDRHandler {
public:
    GDRHandler()
        : width_(0),
        height_(0)
    {
    }

    bool needInsertIdrFrame(int width, int height, int keyFrame)
    {
        //if(width == 0 || height == 0 ) {
        //    return false;
        //}

        // 尺寸发生变化
        if(width != width_ || height != height_) {
            width_ = width;
            height_ = height;

            if(!keyFrame)        // 当前是关键帧，不做处理            
                return true;
        }

        return false;
    }

    void getIdrFrameData(uint8_t **data, uint32_t *len)
    {
        IdrFrameType idrFrameType = IDR_FRAME_TYPE_UNKNOWN;
        if(width_ == 1920 && height_ == 1440) {
            idrFrameType = IDR_FRAME_TYPE_1920x1440;
        }
        else if(width_ == 1920 && height_ == 1080) {
            idrFrameType = IDR_FRAME_TYPE_1920x1080;
        }
        else if(width_ == 1280 && height_ == 720) {
            idrFrameType = IDR_FRAME_TYPE_1280x720;
        }
        else {
            printf("============================== unknow frame size: %dx%d\n", width_, height_);
        }

        H264IdrFrame_GetData(idrFrameType, (const uint8_t **)data, len);
    }

private:
    int width_;
    int height_;
    bool needSpsPps_;

    int threshGop_;
    int sliceCnt_;
};

#endif


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
#ifdef USE_HARDWARE_DECODER
    pCodec_ = avcodec_find_decoder_by_name("h264_cuvid");
#else
    //if(!pCodec_)
        pCodec_ = avcodec_find_decoder(AV_CODEC_ID_H264);
#endif // USE_HARDWARE_DECODER


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

    // ffplay -flags2 showall -i xxxx.h264 可以正常播放GDR视频流
    pCodecCtx_->flags2 |= AV_CODEC_FLAG2_SHOW_ALL;   // 配合CPU可以软解 M300的H20、FPV画面
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
#pragma pack(1)
typedef struct RTP_FIXED_HEADER {
    /* byte 0 */
    unsigned char csrc_len : 4;       /* expect 0 */
    unsigned char extension : 1;      /* expect 1 */
    unsigned char padding : 1;        /* expect 0 */
    unsigned char version : 2;        /* expect 2 */
                                      /* byte 1 */
    unsigned char payload : 7;
    unsigned char marker : 1;        /* expect 1 */
                                     /* bytes 2, 3 */
    unsigned short seq_no;
    /* bytes 4-7 */
    unsigned  long timestamp;
    /* bytes 8-11 */
    unsigned long ssrc;            /* stream number is used here. */
} RTP_FIXED_HEADER;


// PS  FIXED HEADER: 
//   IDR:    PSheader| PS system header | PS system Map | PES header | h264 raw data
//   SLISE:  PSheader| PES header | h264 raw data
typedef struct PS_HEADER {
    uint8_t start_code[4];                // pack start code '000001ba'

    uint8_t left[9];

    //uint8_t  : 2;                       // marker bits '01b'
    //uint8_t sys_clock_ref_2: 3;         // System clock [32..30]
    //uint8_t : 1;                        // marker bits
    //uint16_t sys_clock_ref_1 : 15;      // System clock [29..15]
    //uint8_t : 1;                        // marker bits
    //uint16_t sys_clock_ref_0 : 15;      // System clock [14..0]
    //uint8_t : 1;                        // marker bits

    //uint16_t sys_clock_ref_ext : 9;     // System clock extension [14..0]
    //uint8_t : 1;                        // marker bits

    //uint32_t mux_rate : 22;              // bit rate(n units of 50 bytes per second.)
    //uint8_t : 2;                        // marker bits '11b'

    //uint8_t reserved : 5;               // reserved
    //uint8_t stuffing : 3;               // stuffing length
}PS_HEADER;

typedef struct PS_SYS_HEADER {
    uint8_t start_code[4];                // system header start code  '000001bb'
    uint16_t header_len;                // system header length

    // left                            // left, legth =  header_len - 6
}PS_SYS_HEADER;

typedef struct PS_MAP {
    uint8_t start_code[4];                // start code  '000001bc'

    uint8_t  ps_map_length_1;           // program_stream_map_length = ps_map_length_1 << 8 | ps_map_length_0
    uint8_t  ps_map_length_0;

    uint8_t   program_stream_map_version : 5;
    uint8_t   : 2;
    uint8_t   current_next_indicator : 1;
   
    uint8_t   : 1;
    uint8_t   : 7;

    uint8_t  ps_info_length_1;              // program_stream_info_length = ps_info_length_1 << 8 | ps_info_length_0
    uint8_t  ps_info_length_0;

}PS_MAP;


typedef struct PES_HEADER {
    uint8_t start_code[4];     // 3B '000001';    1B  '0b110x xxxx'-audio    '0b1110 xxxx'-video
    uint8_t pes_length_1;    
    uint8_t pes_length_0;    
    
    /// 基本流特有信息 3~259 Byte
    // 2B 识别标志
    uint16_t identification;

    // 1B pes包头长
    uint8_t header_data_length;

}PES_HEADER;

#pragma pack() 

static_assert(sizeof(RTP_FIXED_HEADER) == 12, "");
static_assert(sizeof(PS_HEADER) == 13, "PS header length error");
static_assert(sizeof(PS_MAP) == 10, "PS map length error");
static_assert(sizeof(PES_HEADER) == 9, "PES header length error");


constexpr int RTP_HEADER_SIZE   = sizeof(RTP_FIXED_HEADER);
constexpr int PS_HEADER_SIZE    = sizeof(PS_HEADER);
constexpr int PS_MAP_SIZE        = sizeof(PS_MAP);
constexpr int PES_HEADER_SIZE   = sizeof(PES_HEADER);

void StreamDecoder::DecodeBuffer(const uint8_t * buf, int len)
{
    //printf("-------------------------------------------------- recv len = %d\n", len);

    int ret;

    uint8_t* pData = const_cast<uint8_t*>(buf);
    int remainingLen = len;
    int processedLen = 0;


    //RTP_FIXED_HEADER *rtpHeader = (RTP_FIXED_HEADER *)buf;
    //switch(rtpHeader->payload) {
    
    //    case 96: { // RTP payload: PS        GB28181 96    ffmpeg 98

    //        pData += RTP_HEADER_SIZE;
    //        remainingLen -= RTP_HEADER_SIZE;

    //        PS_HEADER* psHeader = (PS_HEADER*)pData;
    //        //if(psHeader->start_code == 0xba010000) {   // PS_HEADER
    //        if(pData[0] == 0x00 && pData[1] == 0x00 && pData[2] == 0x01 && pData[3] == 0xba) 
    //        {   // PS_HEADER
    //            printf("start ps packet code\n");

    //            pData += PS_HEADER_SIZE;
    //            remainingLen -= PS_HEADER_SIZE;

    //            uint8_t pack_stuffing_length = *pData & 0x03;
    //            pData +=1;
    //            remainingLen -= 1;
    //            if(pack_stuffing_length > 0) {
    //                pData += pack_stuffing_length * 1;  // stuffing_bytes
    //                remainingLen -= pack_stuffing_length * 1;
    //            }

    //            //if(*(uint32_t*)pData == 0xbb010000) {  // PS_SYS_HEADER
    //            if(remainingLen && pData[0] == 0x00 && pData[1] == 0x00 && pData[2] == 0x01 && pData[3] == 0xbb )
    //            {
    //                pData += 4;
    //                uint16_t psSysLen = *pData << 8 | *(pData+1);
    //                pData += 2 + psSysLen;

    //                remainingLen -= 6 + psSysLen;

    //                PS_MAP* psMapHeader = (PS_MAP*)pData;
    //                //assert(psMapHeader->start_code_prefix == 0xbc010000);
    //                int streamLen = psMapHeader->ps_map_length_1 << 8 | psMapHeader->ps_map_length_0;
    //                pData += PS_MAP_SIZE;
    //                remainingLen -= PS_MAP_SIZE;


    //                const uint8_t* elementryStramMapPtr = pData;
    //                uint16_t esMapLength = *pData << 8 | *(pData + 1);
    //                pData += 2;
    //                
    //                printf("PS Stream map: \n");
    //                //pData += esMapLength;  // 不进行解析
    //                // 进行解析
    //                for(; pData < elementryStramMapPtr + esMapLength;) {
    //                    uint8_t stream_type = *pData;                       pData += 1;
    //                    uint8_t stream_id = *pData;                         pData += 1;
    //                    uint16_t esInfoLen = *pData << 8 | *(pData + 1);    pData += 2;
    //                    if(esInfoLen > 0) {
    //                        pData += esInfoLen;
    //                    }
    //                    if     (stream_type == 0x10)    printf("\t type MPEG4, id: %d\n", stream_id);
    //                    else if(stream_type == 0x1B)    printf("\t type H.264, id: %d\n", stream_id);
    //                    else if(stream_type == 0x80)    printf("\t type SVAC Video, id: %d\n", stream_id);   
    //                    else if(stream_type == 0x90)    printf("\t type G.711, id: %d\n", stream_id);
    //                    else if(stream_type == 0x92)    printf("\t type G.722.1, id: %d\n", stream_id);
    //                    else if(stream_type == 0x93)    printf("\t type G.723.1, id: %d\n", stream_id);
    //                    else if(stream_type == 0x99)    printf("\t type G.729, id: %d\n", stream_id);
    //                    else if(stream_type == 0x9B)    printf("\t type SVAC Audio, id: %d\n", stream_id); 
    //                }
    //                pData += 4;   

    //                remainingLen -= 6 + esMapLength;
    //                
    //                // PES 
    //                if(remainingLen) {

    //                    PES_HEADER * pesHeader = (PES_HEADER *)pData;
    //                    uint8_t header_data_length = pesHeader->header_data_length;

    //                    //TODO.....  一个packet是否含有多个帧

    //                    //if(pesHeader->start_code == 0xe0010000) {   // video
    //                    if(pData[0] == 0x00 && pData[1] == 0x00 && pData[2] == 0x01 && pData[3] == 0xe0) {

    //                        printf("====================  ba bb bc e0 %s\n", rtpHeader->marker ? "   Mark" : "");

    //                        pData += PES_HEADER_SIZE + header_data_length;
    //                        remainingLen -= PES_HEADER_SIZE + header_data_length;

    //                    }
    //                    //if(pesHeader->start_code == 0xc0010000) {   // audio
    //                    else if(pData[0] == 0x00 && pData[1] == 0x00 && pData[2] == 0x01 && pData[3] == 0xc0) {
    //                        // 不处理audio
    //                        printf("====================  ba bb bc c0 (skip) \n");
    //                        return;
    //                    }
    //                }


    //            }
    //            else if(remainingLen )
    //            {
    //                PES_HEADER * pesHeader = (PES_HEADER *)pData;
    //                uint8_t header_data_length = pesHeader->header_data_length;

    //                //TODO.....  一个packet是否含有多个帧

    //                //if(pesHeader->start_code == 0xe0010000) {   // video
    //                if(pData[0] == 0x00 && pData[1] == 0x00 && pData[2] == 0x01 && pData[3] == 0xe0) {

    //                    printf("====================  ba e0 %s\n", rtpHeader->marker ? "   Mark" : "");

    //                    pData += PES_HEADER_SIZE + header_data_length;
    //                    remainingLen -= PES_HEADER_SIZE + header_data_length;

    //                }
    //                //if(pesHeader->start_code == 0xc0010000) {   // audio
    //                else if(pData[0] == 0x00 && pData[1] == 0x00 && pData[2] == 0x01 && pData[3] == 0xc0) {
    //                    // 不处理audio
    //                    printf("====================  ba c0 (skip) \n");
    //                    return;
    //                }
    //            }
    //        }
    //        else {

    //            // PES 
    //            PES_HEADER * pesHeader = (PES_HEADER *)pData;
    //            uint8_t header_data_length = pesHeader->header_data_length;

    //            //TODO.....  一个packet是否含有多个帧

    //            //if(pesHeader->start_code == 0xe0010000) {   // video
    //            if(pData[0] == 0x00 && pData[1] == 0x00 && pData[2] == 0x01 && pData[3] == 0xe0) {

    //                printf("====================  e0 %s\n", rtpHeader->marker ? "   Mark" : "");

    //                pData += PES_HEADER_SIZE + header_data_length;
    //                remainingLen -= PES_HEADER_SIZE + header_data_length;

    //            }
    //            //if(pesHeader->start_code == 0xc0010000) {   // audio
    //            else if(pData[0] == 0x00 && pData[1] == 0x00 && pData[2] == 0x01 && pData[3] == 0xc0) {
    //                // 不处理audio
    //                printf("====================  c0 (skip)\n");
    //                return;
    //            }
    //            else {  // raw data
    //                printf("====================  raw data %s\n", rtpHeader->marker ? "   Mark":"");
    //            }
    //        }

    //    }break;
    //    case 98: { // RTP payload: H264      GB28181 98    ffmpeg 96

    //        ///// ffmpeg.exe -re -i xcp1.mp4 -vcodec copy -an -f rtp udp://192.168.3.100:8000   

    //        pData += RTP_HEADER_SIZE;
    //        remainingLen -= RTP_HEADER_SIZE;

    //        uint8_t naluHeader = *pData;  // NALU Header 或 FU-A Indicator

    //        uint8_t packetType = *pData & 0x1f;



    //        if(packetType < 24) {  // NALU 添加 00000001

    //            printf("NALU type：%d \n", packetType);

    //            pData -= 4;
    //            *(int32_t*)pData = 0x01000000;
    //            remainingLen += 4;  // 前移添加 00000001
    //        }
    //        else if(packetType == 28) {  //

    //            uint8_t fuaHeader = *(++pData);
    //            uint8_t indType = fuaHeader & 0x1f;
    //            uint8_t indS = (fuaHeader & 0x80) >> 7;
    //            uint8_t indE = (fuaHeader & 0x40) >> 6;
    //            printf("FU-A type: %d, S %d, E %d \n", indType, indS, indE);

    //            pData ++; // 指向payload

    //            remainingLen -= 2;  // 后移了2位  ， FU-A Indictor 和 FU-A Header

    //            if(indS == 1) {
    //                pData--;
    //                *pData = ( naluHeader & 0xe0 ) | indType;
    //                pData -= 4;
    //                *(int32_t*)pData = 0x01000000;
    //                remainingLen += 5;  // 前移添加 00000001
    //            }

    //        }
    //        else {
    //            printf("unsupport packet type %d\n", packetType);
    //            return;
    //        }
    //        

    //    }break;
    //    default: {
    //        printf("unknow rtp payload type.\n");
    //        return;
    //    }
    //}
    ////printf("========== rtpHeader->payload %d\n", rtpHeader->payload);
    ////return;

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


        //FILE *pfile = fopen("tmp.h264", "wb");
        //if(pfile) {
        //    fwrite(pPacket_->data, 1, pPacket_->size, pfile);
        //}
        //fclose(pfile);

       // if(pCodecParserCtx_->key_frame)
            printf("parser size: %d x %d, pci_type %d, keyframe %d\n",
                   pCodecParserCtx_->width, pCodecParserCtx_->height, pCodecParserCtx_->pict_type, pCodecParserCtx_->key_frame);

        //continue;

#ifdef USE_HARDWARE_DECODER
        //// GDR 判断及处理 !!!!!!!!!!!!!
        static GDRHandler gdrHandler;      
        bool needInsertIdrFrame = gdrHandler.needInsertIdrFrame(pCodecParserCtx_->width, pCodecParserCtx_->height, pCodecParserCtx_->key_frame);
        if(needInsertIdrFrame) {
            //avcodec_flush_buffers(pCodecCtx_);  // 清空解码器中的缓冲    (提前，尺寸变化时修改 ？？？？？？？？？)
            gdrHandler.getIdrFrameData(&pData, (uint32_t *)&remainingLen);
        }
#endif

        // decoded YUV frame
        ret = avcodec_send_packet(pCodecCtx_, pPacket_);
        while(ret >= 0) {
            ret = avcodec_receive_frame(pCodecCtx_, pFrameYUV_);
            if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                //printf(" codec: %d x %d     ret = %d\n", pCodecCtx_->width, pCodecCtx_->height, ret);
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
            printf("Decoder Callback Thread: Get image time out\n");
            continue;
        }
        
        if(cb_) {
            cb_(copyOfImage);
        }
    }
}
