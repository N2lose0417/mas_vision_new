/*
 * @Author: laladuduqq 2807523947@qq.com
 * @Date: 2025-07-28 18:10:53
 * @LastEditors: laladuduqq 2807523947@qq.com
 * @LastEditTime: 2025-08-31 13:47:40
 * @FilePath: /mas_vision_new/hikcamera/thread/cam_thread.cpp
 * @Description:
 */
#include "HikCamera.h"
#include "OpenCVCamera.h"
#include <opencv2/core/mat.hpp>
#include <opencv2/opencv.hpp>
#include <atomic>
#include <thread>
#include <optional>
#include <algorithm>
#include <cctype>
#include <chrono>
#include "topicqueue.hpp"
#include "udp_comm.hpp"
#include "ulog.hpp"
#include "recorder.hpp"
#include "video.hpp"  
#include "yaml-cpp/yaml.h"

extern std::atomic<bool> running;
extern std::unique_ptr<rm_utils::UDPClient> udpClient;

// 创建一个话题队列实例
rm_utils::TopicQueue<CameraFrame> image_queue(10);  // 每个话题最多存储10帧图像
static std::thread camera_thread;
static bool displayEnabled = false;
static std::atomic<bool> camera_thread_running(false);
static bool recordEnabled = false;
static double recordFps = 30.0;
static std::unique_ptr<rm_utils::Recorder> recorder = nullptr;

// 相机线程函数
void cameraThreadFunc() {
    // 默认参数
    float exposure_time = 5000.0f;
    float gain = 10.0f;
    std::string videoPath = "";
    std::string mode = "webcam"; // 默认使用笔记本摄像头
    double videoFps = 30.0; // 视频播放FPS
    hikcamera::OpenCVCamera::Options webcamOptions;
    std::string webcamFlipMode = "none";
    std::string webcamRotateMode = "none";

    auto toLower = [](std::string value) {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return value;
    };

    // 在相机线程中读取YAML配置文件
    try {
        YAML::Node config = YAML::LoadFile("config/camera_set.yaml");
        if (config["camera"]) {
            exposure_time = config["camera"]["exposuretime"].as<float>(5000.0f);
            gain = config["camera"]["gain"].as<float>(10.0f);
            ULOG_INFO_TAG("Camera","已加载相机参数");
        }

        if (config["webcam"]) {
            auto webcam = config["webcam"]; 
            if (webcam["device_index"]) {
                webcamOptions.device_index = webcam["device_index"].as<int>(webcamOptions.device_index);
            }
            if (webcam["width"]) {
                webcamOptions.frame_width = webcam["width"].as<int>(webcamOptions.frame_width);
            }
            if (webcam["height"]) {
                webcamOptions.frame_height = webcam["height"].as<int>(webcamOptions.frame_height);
            }
            if (webcam["fps"]) {
                webcamOptions.fps = webcam["fps"].as<double>(webcamOptions.fps);
            }
            if (webcam["auto_exposure"]) {
                webcamOptions.auto_exposure = webcam["auto_exposure"].as<bool>(webcamOptions.auto_exposure);
            }
            if (webcam["exposure"]) {
                webcamOptions.exposure = webcam["exposure"].as<double>(webcamOptions.exposure);
            }
            if (webcam["gain"]) {
                webcamOptions.gain = webcam["gain"].as<double>(webcamOptions.gain);
            }
            if (webcam["auto_white_balance"]) {
                webcamOptions.auto_white_balance = webcam["auto_white_balance"].as<bool>(webcamOptions.auto_white_balance);
            }
            if (webcam["white_balance"]) {
                webcamOptions.white_balance = webcam["white_balance"].as<double>(webcamOptions.white_balance);
            }
            if (webcam["auto_focus"]) {
                webcamOptions.auto_focus = webcam["auto_focus"].as<bool>(webcamOptions.auto_focus);
            }
            if (webcam["focus"]) {
                webcamOptions.focus = webcam["focus"].as<double>(webcamOptions.focus);
            }
            if (webcam["auto_saturation"]) {
                webcamOptions.auto_saturation = webcam["auto_saturation"].as<bool>(webcamOptions.auto_saturation);
            }
            if (webcam["saturation"]) {
                webcamOptions.saturation = webcam["saturation"].as<double>(webcamOptions.saturation);
            }
            if (webcam["warmup_frames"]) {
                webcamOptions.warmup_frames = webcam["warmup_frames"].as<int>(webcamOptions.warmup_frames);
            }
            if (webcam["drop_initial_frames"]) {
                webcamOptions.drop_initial_frames = webcam["drop_initial_frames"].as<bool>(webcamOptions.drop_initial_frames);
            }
            if (webcam["backend"]) {
                std::string backend = toLower(webcam["backend"].as<std::string>("auto"));
                if (backend == "auto") {
                    webcamOptions.backend = cv::CAP_ANY;
                } else if (backend == "v4l" || backend == "v4l2") {
                    webcamOptions.backend = cv::CAP_V4L2;
                } else if (backend == "gstreamer") {
                    webcamOptions.backend = cv::CAP_GSTREAMER;
                } else if (backend == "dshow") {
                    webcamOptions.backend = cv::CAP_DSHOW;
                } else if (backend == "msmf" || backend == "mediafoundation") {
                    webcamOptions.backend = cv::CAP_MSMF;
                } else if (backend == "avfoundation") {
                    webcamOptions.backend = cv::CAP_AVFOUNDATION;
                }
            }
            if (webcam["rotate"]) {
                webcamRotateMode = webcam["rotate"].as<std::string>(webcamRotateMode);
            }
            if (webcam["flip"]) {
                webcamFlipMode = webcam["flip"].as<std::string>(webcamFlipMode);
            }
        }

        // 读取显示配置
        if (config["display"]) {
            displayEnabled = config["display"].as<bool>(false);
        }
        
        // 读取录制配置
        if (config["record"]) {
            recordEnabled = config["record"]["enabled"].as<bool>(false);
            recordFps = config["record"]["fps"].as<double>(30.0);
        }
        
        // 读取模式配置
        if (config["mode"]) {
            mode = toLower(config["mode"].as<std::string>("webcam"));
        }

        // 读取视频路径和FPS（如果模式是video）
        if (config["video"]) {
            if (config["video"]["path"]) {
                videoPath = config["video"]["path"].as<std::string>("");
            }
            if (config["video"]["fps"]) {
                videoFps = config["video"]["fps"].as<double>(30.0);
            }
        }
    } catch (const std::exception& e) {
        ULOG_ERROR_TAG("Camera", "Failed to load camera config: %s", e.what());
    };
    
    bool initSuccess = false;

    const auto flipCode = [&]() -> std::optional<int> {
        std::string value = toLower(webcamFlipMode);
        if (value.empty() || value == "none") {
            return std::nullopt;
        }
        if (value == "horizontal" || value == "h" || value == "x") {
            return 1;
        }
        if (value == "vertical" || value == "v" || value == "y") {
            return 0;
        }
        if (value == "both" || value == "hv" || value == "xy") {
            return -1;
        }
        return std::nullopt;
    }();

    const auto rotateCode = [&]() -> std::optional<int> {
        std::string value = toLower(webcamRotateMode);
        if (value.empty() || value == "none") {
            return std::nullopt;
        }
        if (value == "90" || value == "cw" || value == "clockwise" || value == "clockwise90") {
            return cv::ROTATE_90_CLOCKWISE;
        }
        if (value == "270" || value == "ccw" || value == "counterclockwise" || value == "counterclockwise90") {
            return cv::ROTATE_90_COUNTERCLOCKWISE;
        }
        if (value == "180") {
            return cv::ROTATE_180;
        }
        return std::nullopt;
    }();

    auto backendName = [](int backend) -> std::string {
        switch (backend) {
            case cv::CAP_ANY: return "auto";
            case cv::CAP_V4L2: return "v4l2";
            case cv::CAP_GSTREAMER: return "gstreamer";
            case cv::CAP_DSHOW: return "dshow";
            case cv::CAP_MSMF: return "msmf";
            case cv::CAP_AVFOUNDATION: return "avfoundation";
            default: return std::to_string(backend);
        }
    };

    // 根据配置选择模式
    if (mode == "video" && !videoPath.empty()) {
        // 视频模式
        ULOG_INFO_TAG("Camera", "使用视频模式，视频路径: %s, FPS: %f", videoPath.c_str(), videoFps);
        rm_utils::VideoPlayer videoPlayer;
        // 设置视频播放FPS
        videoPlayer.setTargetFPS(videoFps);
        initSuccess = videoPlayer.open(videoPath);
        
        if (initSuccess) {
            ULOG_INFO_TAG("Camera", "视频初始化成功");
            
            // 如果启用了录制，创建Recorder实例
            if (recordEnabled) {
                recorder = std::make_unique<rm_utils::Recorder>(recordFps);
                ULOG_INFO_TAG("Camera", "Recorder initialized with FPS: %f", recordFps);
            }
            
            while (running.load() && camera_thread_running.load()) {
                cv::Mat frame;
                if (videoPlayer.read(frame)) {
                    auto timestamp = std::chrono::steady_clock::now();
                    // 创建带时间戳的帧
                    CameraFrame timeCameraFrame{frame, timestamp};
                    // 发布图像消息
                    image_queue.push("image/camera", timeCameraFrame);
                    
                    // 如果启用了录制，进行录制
                    if (recordEnabled && recorder) {
                        recorder->record(frame);
                    }
                    
                    if (displayEnabled) {
                        // 显示图像
                        cv::Mat resizedDrawingFrame;
                        cv::resize(frame, resizedDrawingFrame, cv::Size(frame.cols/2, frame.rows/2), 0, 0, cv::INTER_LINEAR);
                        if (udpClient && udpClient->isInitialized()) {
                            udpClient->sendImage(resizedDrawingFrame,"camera");
                        }
                    }
                } 
            }
            videoPlayer.close();
        }
    } else if (mode == "camera") {
#ifdef HAS_HIK_CAMERA
        // 相机模式（默认）
        ULOG_INFO_TAG("Camera", "使用相机模式");

        // 在相机线程内部创建相机对象，传入读取到的参数
        hikcamera::HikCamera cam(exposure_time, gain);
        initSuccess = cam.openCamera();
        
        // 初始化相机
        if (initSuccess) {
            ULOG_INFO_TAG("Camera","相机初始化成功");

            // 如果启用了录制，创建Recorder实例
            if (recordEnabled) {
                recorder = std::make_unique<rm_utils::Recorder>(recordFps);
                ULOG_INFO_TAG("Camera", "Recorder initialized with FPS: %f", recordFps);
            }

            while (running.load() && camera_thread_running.load()) {
                cv::Mat frame;
                // 获取帧
                if (cam.grabImage(frame)) {
                    auto timestamp = std::chrono::steady_clock::now();
                    // 创建带时间戳的帧
                    CameraFrame timeCameraFrame{frame, timestamp};
                    // 发布图像消息
                    image_queue.push("image/camera", timeCameraFrame);
                    
                    // 如果启用了录制，进行录制
                    if (recordEnabled && recorder) {
                        recorder->record(frame);
                    }
                    
                    if (displayEnabled) {
                        // 显示图像
                        cv::Mat resizedDrawingFrame;
                        cv::resize(frame, resizedDrawingFrame, cv::Size(frame.cols/2, frame.rows/2), 0, 0, cv::INTER_LINEAR);
                        if (udpClient && udpClient->isInitialized()) {
                            udpClient->sendImage(resizedDrawingFrame,"camera");
                        }
                    }
                }
            }
            cam.closeCamera();
        }
#else
        ULOG_ERROR_TAG("Camera", "Hikvision SDK is not available. Falling back to webcam mode.");
        mode = "webcam";
#endif
    }

    if (!initSuccess && mode != "video") {
        // 默认使用webcam模式
        ULOG_INFO_TAG("Camera", "使用webcam模式，设备: %d, 分辨率: %dx%d, FPS: %.2f, 后端: %s",
                      webcamOptions.device_index, webcamOptions.frame_width, webcamOptions.frame_height,
                      webcamOptions.fps, backendName(webcamOptions.backend).c_str());

        hikcamera::OpenCVCamera webcam(webcamOptions);
        initSuccess = webcam.open();

        if (initSuccess) {
            if (flipCode) {
                ULOG_INFO_TAG("Camera", "Webcam 图像翻转模式: %d", *flipCode);
            }
            if (rotateCode) {
                ULOG_INFO_TAG("Camera", "Webcam 图像旋转模式: %d", *rotateCode);
            }

            if (recordEnabled) {
                recorder = std::make_unique<rm_utils::Recorder>(recordFps);
                ULOG_INFO_TAG("Camera", "Recorder initialized with FPS: %f", recordFps);
            }

            while (running.load() && camera_thread_running.load()) {
                cv::Mat frame;
                if (webcam.grabImage(frame)) {
                    if (rotateCode) {
                        cv::rotate(frame, frame, *rotateCode);
                    }
                    if (flipCode) {
                        cv::flip(frame, frame, *flipCode);
                    }

                    auto timestamp = std::chrono::steady_clock::now();
                    CameraFrame timeCameraFrame{frame, timestamp};
                    image_queue.push("image/camera", timeCameraFrame);

                    if (recordEnabled && recorder) {
                        recorder->record(frame);
                    }

                    if (displayEnabled) {
                        cv::Mat resizedDrawingFrame;
                        cv::resize(frame, resizedDrawingFrame, cv::Size(frame.cols/2, frame.rows/2), 0, 0, cv::INTER_LINEAR);
                        if (udpClient && udpClient->isInitialized()) {
                            udpClient->sendImage(resizedDrawingFrame,"camera");
                        }
                    }
                }
            }
        }
    }

    if (!initSuccess) {
        ULOG_ERROR_TAG("Camera","Failed to initialize camera or open video");
        running = false;
        return;
    }
    
    // 重置recorder指针，确保正确析构
    recorder.reset();
}

// 启动函数
void startCameraThread() {    
    camera_thread_running = true;
    camera_thread = std::thread(cameraThreadFunc);
}

// 停止函数
void stopCameraThread() {
    if (!camera_thread_running) {
        ULOG_WARNING_TAG("Camera", "Camera thread not running");
        return;
    }
    camera_thread_running = false;
    if (camera_thread.joinable()) {
        camera_thread.join();
    }
    ULOG_INFO_TAG("Camera", "Camera thread stopped");
}