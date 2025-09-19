#pragma once

#include <opencv2/core/mat.hpp>
#include <opencv2/videoio.hpp>
#include <mutex>

namespace hikcamera {

class OpenCVCamera {
public:
    struct Options {
        int device_index = 0;
        int frame_width = 1280;
        int frame_height = 720;
        double fps = 60.0;
        bool auto_exposure = true;
        double exposure = -4.0;  // OpenCV exposure units (log scale for many drivers)
        double gain = 0.0;
        bool auto_white_balance = true;
        double white_balance = 4600.0;
        bool auto_focus = true;
        double focus = 0.0;
        bool auto_saturation = true;
        double saturation = 64.0;
        int warmup_frames = 15;
        bool drop_initial_frames = true;
        int backend = cv::CAP_ANY;
    };

    explicit OpenCVCamera(Options options = Options());
    ~OpenCVCamera();

    bool open();
    void close();
    bool isOpened() const;
    bool grabImage(cv::Mat& frame);

private:
    bool applyOptions();
    bool setProperty(int prop_id, double value, const char* name) const;

    Options options_;
    cv::VideoCapture cap_;
    mutable std::mutex capture_mutex_;
    bool options_applied_ = false;
};

}  // namespace hikcamera

