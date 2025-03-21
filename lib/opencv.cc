#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

void opencv_resize_area(uint8_t* src, uint8_t* dst, int in_w, int in_h, int out_w, int out_h) {
    // Convert to OpenCV Mat
    cv::Mat input_mat(in_h, in_w, CV_8UC1, src);  // Assuming grayscale, adjust CV_8UC3 for RGB
    cv::Mat output_mat(out_h, out_w, CV_8UC1, dst);

    // Perform resizing using OpenCV's INTER_AREA (better for downscaling)
    cv::resize(input_mat, output_mat, cv::Size(out_w, out_h), 0, 0, cv::INTER_AREA);
}