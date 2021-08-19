#pragma once

#include "Image.h"

#include <mutex>
#include <condition_variable>

template<typename T>
class ImageHandler
{
public:
    ImageHandler();
    ~ImageHandler();

    bool NewImageIsReady() { return newImgFlag_; }

    void WriteNewImage(const uint8_t* buf, int bufSize, int width, int height);

    bool GetNewImage(T& copyOfImage, int timoeoutMillSec);


private:
    std::mutex              mtx_;
    std::condition_variable cv_;
    T                       img_;
    bool                    newImgFlag_;
};


using RGBImageHandler = ImageHandler<RGBImage>;