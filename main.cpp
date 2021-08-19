#include <iostream>

#include "StreamApp.h"

// FileHandler

class FileHandler {
public:
    FileHandler(const char* filename)
    {
        fp = fopen(filename, "wb");
        if(!fp)
            printf("open file failed.\n");
    }

    ~FileHandler()
    {
        fclose(fp);
    }

    void callback(const uint8_t* buf, int len) 
    {
        printf("\rrecv buf len %d", len);
        if(fp) 
            fwrite(buf, 1, len, fp);       
    }
private:
    FILE *fp;
};


// opencv show
#include "opencv2/opencv.hpp"
class OpencvHandler {
public:
    OpencvHandler(const char* name, int mode = 1)
    { 
        winName = std::string(name);
        cv::namedWindow(winName, mode); 
    }
    ~OpencvHandler(){   
        cv::destroyWindow(winName);
    }

    void callback(RGBImage img)
    {
        printf("recv rgb img %dx%d, bytes %d, data ptr %p\n", 
               img.height, img.width, img.rawData.size(), img.rawData.data());

        cv::Mat mat(img.height, img.width, CV_8UC3, const_cast<uchar *>(img.rawData.data()));

        //cv::imshow(winName, mat);   // 不能跨线程使用
        //cv::waitKey(1);             
    }

private:
    std::string winName;
};



int testUdpServer()
{
    UdpServer* udpServer_ = new UdpServer();
    udpServer_->Init("192.168.3.100",8000);
    udpServer_->RegisterCallback([&](const uint8_t* buf, int len) {
        //if(streamDecoder_)
        //    streamDecoder_->DecodeBuffer(buf, len);
        printf("\r recv packet %d", len);
    });
    udpServer_->Start();

    getchar();
    udpServer_->RegisterCallback(NULL);
    getchar();
    udpServer_->Cleanup();

    delete udpServer_;

    return 0;
}


int testStreamDecoder()
{
    {
        printf("One ----------------------------------------\n");

        StreamDecoder* streamDecoder_ = new StreamDecoder();
        streamDecoder_->Init();
        streamDecoder_->RegisterCallback([](RGBImage img) {
            printf("\r recv img %dx%d", img.width, img.height);
        });

        getchar();
        streamDecoder_->RegisterCallback(NULL);
        getchar();
        streamDecoder_->Cleanup();
        delete streamDecoder_;
    }

    getchar();
    printf(" two ----------------------------------------\n");

    {
        StreamDecoder* streamDecoder_ = new StreamDecoder();
        streamDecoder_->Init();
        streamDecoder_->RegisterCallback([](RGBImage img) {
            printf("\r recv img %dx%d", img.width*img.height);
        });

        UdpServer* udpServer_ = new UdpServer();
        udpServer_->Init("192.168.3.100", 8000);
        udpServer_->RegisterCallback([&](const uint8_t* buf, int len) {
            //if(streamDecoder_)
            //    streamDecoder_->DecodeBuffer(buf, len);
            printf("\r recv packet %d", len);
        });
        udpServer_->Start();


        getchar();
        udpServer_->RegisterCallback(NULL);
        streamDecoder_->RegisterCallback(NULL);
        getchar();
        streamDecoder_->Cleanup();
        udpServer_->Cleanup();

        delete udpServer_;
        delete streamDecoder_;
    }

    return 0;
}


int main()
{
    //  ffmpeg -re -i xxxxxx.h264 -vcodec copy     -f h264 udp://192.168.3.100:8000
    //  ffmpeg -re -i xxxxxx.mp4  -vcodec copy -an -f h264 udp://192.168.3.100:8000


    //return testUdpServer();
    //return testStreamDecoder();

    using std::placeholders::_1;
    using std::placeholders::_2;

    StreamApp app("192.168.3.100", 8000);
    
    if(0){
        FileHandler filehandle("out.h264");
        auto cb = std::bind(&FileHandler::callback, &filehandle, _1, _2);

        app.StartH264Stream(cb);
        getchar();
        app.StopH264Stream();
    }

    if(1) {
        //OpencvHandler opencvHandler("img");
        //auto cb = std::bind(&OpencvHandler::callback, &opencvHandler, _1);
        //app.StartImageStream(cb);

        app.StartImageStream([](RGBImage img){
            printf("\rrecv rgb img %dx%d, bytes %d, data ptr %p         ",
                   img.height, img.width, img.rawData.size(), img.rawData.data());

            cv::Mat mat(img.height, img.width, CV_8UC3, const_cast<uchar *>(img.rawData.data()));
            cv::imshow("img", mat);
            cv::waitKey(1);
        });

        getchar();
        app.StopImageStream();
    }


    if(0) {   
        app.StartImageStream(); // 不设置回调

        RGBImage img;
        for(int i = 1000; i > 0; --i) {  // 轮询1000次
            if(app.NewImageIsReady()) {
                if(app.GetCurrentImage(img)) {
                    printf("\rrecv rgb img %dx%d, bytes %d, data ptr %p         ",
                           img.height, img.width, img.rawData.size(), img.rawData.data());
                    cv::Mat mat(img.height, img.width, CV_8UC3, const_cast<uchar *>(img.rawData.data()));
                    cv::imshow("img", mat);
                    cv::waitKey(1);
                } else {
                    printf("\t Get Image Timeout                                     ");
                }
            }
            _sleep(20); //20ms  stdlib.h
        }

        app.StopImageStream();
    }

    return 0;
}