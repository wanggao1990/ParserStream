#pragma once

#include <stdint.h>
#include <vector>

#include <functional>

typedef struct RGBImage {
    std::vector<uint8_t> rawData;
    int height;
    int width;
}RGBImage;

using ImageCallback = std::function<void(RGBImage)>;
using H264Callback = std::function<void(const uint8_t*, int)>;