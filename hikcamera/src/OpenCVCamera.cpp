#include "OpenCVCamera.h"

#include "ulog.hpp"

#include <algorithm>
#include <chrono>
#include <thread>
#include <utility>

namespace hikcamera {

OpenCVCamera::OpenCVCamera(Options options) : options_(std::move(options)) {}

OpenCVCamera::~OpenCVCamera() { close(); }

bool OpenCVCamera::open() {
    std::lock_guard<std::mutex> lock(capture_mutex_);
    if (cap_.isOpened()) {
        return true;
    }

    if (options_.backend != cv::CAP_ANY) {
        cap_.open(options_.device_index, options_.backend);
    } else {
        cap_.open(options_.device_index);
    }

    if (!cap_.isOpened()) {
        ULOG_ERROR_TAG("Webcam", "Failed to open video device %d", options_.device_index);
        return false;
    }

    if (!applyOptions()) {
        ULOG_WARNING_TAG("Webcam", "Failed to apply some camera properties, continuing with defaults");
    }

    options_applied_ = true;

    if (options_.drop_initial_frames) {
        const int warmup_frames = std::max(0, options_.warmup_frames);
        cv::Mat frame;
        for (int i = 0; i < warmup_frames; ++i) {
            if (!cap_.read(frame)) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    return true;
}

void OpenCVCamera::close() {
    std::lock_guard<std::mutex> lock(capture_mutex_);
    if (cap_.isOpened()) {
        cap_.release();
        options_applied_ = false;
    }
}

bool OpenCVCamera::isOpened() const {
    std::lock_guard<std::mutex> lock(capture_mutex_);
    return cap_.isOpened();
}

bool OpenCVCamera::grabImage(cv::Mat& frame) {
    std::lock_guard<std::mutex> lock(capture_mutex_);
    if (!cap_.isOpened()) {
        return false;
    }

    bool success = cap_.read(frame);
    if (!success || frame.empty()) {
        return false;
    }

    return true;
}

bool OpenCVCamera::setProperty(int prop_id, double value, const char* name) const {
    if (!cap_.set(prop_id, value)) {
        ULOG_DEBUG_TAG("Webcam", "Failed to set %s to %.2f", name, value);
        return false;
    }
    return true;
}

bool OpenCVCamera::applyOptions() {
    bool ok = true;
    ok &= setProperty(cv::CAP_PROP_FRAME_WIDTH, static_cast<double>(options_.frame_width), "FRAME_WIDTH");
    ok &= setProperty(cv::CAP_PROP_FRAME_HEIGHT, static_cast<double>(options_.frame_height), "FRAME_HEIGHT");
    if (options_.fps > 0) {
        ok &= setProperty(cv::CAP_PROP_FPS, options_.fps, "FPS");
    }

    if (options_.auto_exposure) {
        // OpenCV uses 0.75 for auto exposure on some drivers, 1 on others.
        if (!cap_.set(cv::CAP_PROP_AUTO_EXPOSURE, 1)) {
            cap_.set(cv::CAP_PROP_AUTO_EXPOSURE, 0.75);
        }
    } else {
        // Manual exposure often expects value in log scale (negative numbers)
        cap_.set(cv::CAP_PROP_AUTO_EXPOSURE, 0.25);
        if (options_.exposure != 0.0) {
            setProperty(cv::CAP_PROP_EXPOSURE, options_.exposure, "EXPOSURE");
        }
    }

    if (!options_.auto_focus) {
        cap_.set(cv::CAP_PROP_AUTOFOCUS, 0);
        setProperty(cv::CAP_PROP_FOCUS, options_.focus, "FOCUS");
    } else {
        cap_.set(cv::CAP_PROP_AUTOFOCUS, 1);
    }

    if (!options_.auto_white_balance) {
        cap_.set(cv::CAP_PROP_AUTO_WB, 0);
        setProperty(cv::CAP_PROP_WB_TEMPERATURE, options_.white_balance, "WHITE_BALANCE");
    } else {
        cap_.set(cv::CAP_PROP_AUTO_WB, 1);
    }

    if (!options_.auto_saturation) {
        setProperty(cv::CAP_PROP_SATURATION, options_.saturation, "SATURATION");
    }

    if (options_.gain > 0.0) {
        setProperty(cv::CAP_PROP_GAIN, options_.gain, "GAIN");
    }

    return ok;
}

}  // namespace hikcamera

