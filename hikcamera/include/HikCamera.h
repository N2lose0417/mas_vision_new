/*
 * @Author: laladuduqq 2807523947@qq.com
 * @Date: 2025-07-27 17:37:26
 * @LastEditors: laladuduqq 2807523947@qq.com
 * @LastEditTime: 2025-08-22 10:38:41
 * @FilePath: /mas_vision_new/hikcamera/include/HikCamera.h
 * @Description: 
 */
#ifndef HIKCAMERA_H
#define HIKCAMERA_H

#include <stdio.h>
#include <string.h>
#include <chrono>
#include <opencv2/opencv.hpp>

#ifdef HAS_HIK_CAMERA
#include "MvCameraControl.h"
#endif

struct CameraFrame {
    cv::Mat frame;
    std::chrono::steady_clock::time_point timestamp;
};

namespace hikcamera {
#ifdef HAS_HIK_CAMERA
    class HikCamera {
    public:
        HikCamera(float exposure_time, float gain);
        ~HikCamera();

        bool openCamera();
        void closeCamera();
        bool grabImage(cv::Mat& outImg);

    private:
        void* handle;           // Camera handle
        bool isConnected;       // Connection status
        char serialNumber[64];  // Device serial number
        unsigned int g_nPayloadSize; // Payload size for image buffer
        bool PrintDeviceInfo(MV_CC_DEVICE_INFO* pstMVDevInfo);
        int fail_conut_ = 0;

        // 添加配置参数成员变量
        float exposure_time=5000.0f;
        float gain=10.0f;
    };
#else
    class HikCamera {
    public:
        HikCamera(float, float) {}
        ~HikCamera() = default;

        bool openCamera() { return false; }
        void closeCamera() {}
        bool grabImage(cv::Mat&) { return false; }
    };
#endif
}

#endif // HIKCAMERA_H