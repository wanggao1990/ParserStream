#include "ImageHandler.h"


template<typename T>
ImageHandler<T>::ImageHandler() : newImgFlag_(false)
{
}

template<typename T>
ImageHandler<T>::~ImageHandler()
{
}

template<typename T>
void ImageHandler<T>::WriteNewImage(const uint8_t* buf, int bufSize, int width, int height)
{
    std::unique_lock<std::mutex> lck(mtx_);

    img_.rawData.assign(buf, buf + bufSize);
    img_.width = width;
    img_.height = height;

    newImgFlag_ = true;

    lck.unlock();

    cv_.notify_one();  
}

template<typename T>
bool ImageHandler<T>::GetNewImage(T & copyOfImage, int timoeoutMillSec)
{
    bool ret = false;
    {
        std::unique_lock<std::mutex> lck(mtx_);
        if(newImgFlag_) {
            copyOfImage = img_;
            newImgFlag_ = false;
            ret = true;
        }
        else {
            auto status = cv_.wait_for(lck, std::chrono::milliseconds(timoeoutMillSec), [&]{ return newImgFlag_; });
            if(status) {
                copyOfImage = img_;
                newImgFlag_ = false;
                ret = true;
            }
        }
    }
    return ret;
}


template class ImageHandler<RGBImage>;