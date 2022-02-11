#pragma once

#include <stdint.h>

enum IdrFrameType {
    IDR_FRAME_TYPE_UNKNOWN,
    IDR_FRAME_TYPE_1920x1440,       // IDR_FRAME_TYPE_H20_SHOT_PHOTO
    IDR_FRAME_TYPE_1920x1080,       // IDR_FRAME_TYPE_H20_RECORD_VIDEO
    IDR_FRAME_TYPE_1280x720,        // IDR_FRAME_TYPE_Z30_SHOT_PHOTO, IDR_FRAME_TYPE_Z30_RECORD_VIDEO
};

// M300, FPV, 1280x960


int H264IdrFrame_GetData(IdrFrameType idrFrameType, const uint8_t **data, uint32_t *dataLen);