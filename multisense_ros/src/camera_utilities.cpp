/**
 * @file camera_utilities.cpp
 *
 * Copyright 2020
 * Carnegie Robotics, LLC
 * 4501 Hatfield Street, Pittsburgh, PA 15201
 * http://www.carnegierobotics.com
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Carnegie Robotics, LLC nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL CARNEGIE ROBOTICS, LLC BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **/

#include <sensor_msgs/distortion_models.hpp>

#include <multisense_ros/camera_utilities.h>

namespace multisense_ros {

cv::Matx44d makeQ(const crl::multisense::image::Config& config,
                  const crl::multisense::image::Calibration& calibration,
                  const crl::multisense::system::DeviceInfo& device_info)
{
    cv::Matx44d q_matrix;

    const double x_scale = 1.0 / ((static_cast<double>(device_info.imagerWidth) /
                                   static_cast<double>(config.width())));

    //
    // Compute the Q matrix here, as image_geometery::StereoCameraModel does
    // not allow for non-square pixels.
    //
    //  FyTx    0     0    -FyCxTx
    //   0     FxTx   0    -FxCyTx
    //   0      0     0     FxFyTx
    //   0      0    -Fy    Fy(Cx - Cx')

    q_matrix(0,0) =  config.fy() * config.tx();
    q_matrix(1,1) =  config.fx() * config.tx();
    q_matrix(0,3) = -config.fy() * config.cx() * config.tx();
    q_matrix(1,3) = -config.fx() * config.cy() * config.tx();
    q_matrix(2,3) =  config.fx() * config.fy() * config.tx();
    q_matrix(3,2) = -config.fy();
    q_matrix(3,3) =  config.fy() * (calibration.left.P[0][2] * x_scale - calibration.right.P[0][2] * x_scale);

    return q_matrix;
}

sensor_msgs::msg::CameraInfo makeCameraInfo(const crl::multisense::image::Config& config,
                                            const crl::multisense::image::Calibration::Data& calibration,
                                            const crl::multisense::system::DeviceInfo& device_info)
{
    const double x_scale = 1.0 / ((static_cast<double>(device_info.imagerWidth) /
                                   static_cast<double>(config.width())));
    const double y_scale = 1.0 / ((static_cast<double>(device_info.imagerHeight) /
                                   static_cast<double>(config.height())));

    sensor_msgs::msg::CameraInfo camera_info;

    camera_info.p[0] = calibration.P[0][0] * x_scale;
    camera_info.p[1] = calibration.P[0][1];
    camera_info.p[2] = calibration.P[0][2] * x_scale;
    camera_info.p[3] = calibration.P[0][3] * x_scale;
    camera_info.p[4] = calibration.P[1][0];
    camera_info.p[5] = calibration.P[1][1] * y_scale;
    camera_info.p[6] = calibration.P[1][2] * y_scale;
    camera_info.p[7] = calibration.P[1][3];
    camera_info.p[8] = calibration.P[2][0];
    camera_info.p[9] = calibration.P[2][1];
    camera_info.p[10] = calibration.P[2][2];
    camera_info.p[11] = calibration.P[2][3];

    camera_info.k[0] = calibration.M[0][0] * x_scale;
    camera_info.k[1] = calibration.M[0][1];
    camera_info.k[2] = calibration.M[0][2] * x_scale;
    camera_info.k[3] = calibration.M[1][0];
    camera_info.k[4] = calibration.M[1][1] * y_scale;
    camera_info.k[5] = calibration.M[1][2] * y_scale;
    camera_info.k[6] = calibration.M[2][0];
    camera_info.k[7] = calibration.M[2][1];
    camera_info.k[8] = calibration.M[2][2];

    camera_info.r[0] = calibration.R[0][0];
    camera_info.r[1] = calibration.R[0][1];
    camera_info.r[2] = calibration.R[0][2];
    camera_info.r[3] = calibration.R[1][0];
    camera_info.r[4] = calibration.R[1][1];
    camera_info.r[5] = calibration.R[1][2];
    camera_info.r[6] = calibration.R[2][0];
    camera_info.r[7] = calibration.R[2][1];
    camera_info.r[8] = calibration.R[2][2];

    //
    // Distortion coefficients follow OpenCV's convention.
    // k1, k2, p1, p2, k3, k4, k5, k6

    camera_info.d.resize(8);
    for (size_t i=0 ; i < 8 ; ++i)
    {
        camera_info.d[i] = calibration.D[i];
    }

    //
    // MultiSense cameras support both the full 8 parameter rational_polynomial
    // model and the simplified 5 parameter plum_bob model. If the last 3
    // parameters of the distortion model are 0 then the camera is using
    // the simplified plumb_bob model

    if (calibration.D[7] == 0.0 && calibration.D[6] == 0.0 && calibration.D[5] == 0.0)
    {
        camera_info.distortion_model = sensor_msgs::distortion_models::PLUMB_BOB;
    }
    else
    {
        camera_info.distortion_model = sensor_msgs::distortion_models::RATIONAL_POLYNOMIAL;
    }

    return camera_info;
}

RectificationRemapT makeRectificationRemap(const crl::multisense::image::Config& config,
                                           const crl::multisense::image::Calibration::Data& calibration,
                                           const crl::multisense::system::DeviceInfo& device_info)
{
    RectificationRemapT remap;

    const double x_scale = 1.0 / ((static_cast<double>(device_info.imagerWidth) /
                                   static_cast<double>(config.width())));
    const double y_scale = 1.0 / ((static_cast<double>(device_info.imagerHeight) /
                                   static_cast<double>(config.height())));

    const cv::Matx33d K(calibration.M[0][0] * x_scale,
                        calibration.M[0][1],
                        calibration.M[0][2] * x_scale,
                        calibration.M[1][0],
                        calibration.M[1][1] * y_scale,
                        calibration.M[1][2] * y_scale,
                        calibration.M[2][0],
                        calibration.M[2][1],
                        calibration.M[2][2]);

    const cv::Matx33d R(calibration.R[0][0],
                        calibration.R[0][1],
                        calibration.R[0][2],
                        calibration.R[1][0],
                        calibration.R[1][1],
                        calibration.R[1][2],
                        calibration.R[2][0],
                        calibration.R[2][1],
                        calibration.R[2][2]);

    const cv::Matx34d P(calibration.P[0][0] * x_scale,
                        calibration.P[0][1],
                        calibration.P[0][2] * x_scale,
                        calibration.P[0][3] * x_scale,
                        calibration.P[1][0],
                        calibration.P[1][1] * y_scale,
                        calibration.P[1][2] * y_scale,
                        calibration.P[1][3],
                        calibration.P[2][0],
                        calibration.P[2][1],
                        calibration.P[2][2],
                        calibration.P[2][3]);

    const bool plumbob = calibration.D[7] == 0.0 && calibration.D[6] == 0.0 && calibration.D[5] == 0.0;

    cv::Mat D(plumbob ? 5 : 8, 1, CV_64FC1);
    for (int i = 0; i < D.rows ; ++i)
    {
        D.at<double>(i) = calibration.D[i];
    }

    cv::initUndistortRectifyMap(K, D, R, P, cv::Size(config.width(), config.height()), CV_32FC1, remap.map1, remap.map2);

    return remap;
}

StereoCalibrationManger::StereoCalibrationManger(const crl::multisense::image::Config& config,
                                                 const crl::multisense::image::Calibration& calibration,
                                                 const crl::multisense::system::DeviceInfo& device_info):
    config_(config),
    calibration_(calibration),
    device_info_(device_info),
    q_matrix_(makeQ(config_, calibration_, device_info_)),
    left_camera_info_(makeCameraInfo(config_, calibration_.left, device_info_)),
    right_camera_info_(makeCameraInfo(config_, calibration_.right, device_info_)),
    left_remap_(makeRectificationRemap(config_, calibration_.left, device_info_)),
    right_remap_(makeRectificationRemap(config_, calibration_.right, device_info_))
{
}

void StereoCalibrationManger::updateConfig(const crl::multisense::image::Config& config)
{
    //
    // Only update the calibration if the resolution changed.

    if (config_.width() == config.width() && config_.height() == config.height())
    {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;
        return;
    }

    auto q_matrix = makeQ(config, calibration_, device_info_);
    auto left_camera_info = makeCameraInfo(config, calibration_.left, device_info_);
    auto right_camera_info = makeCameraInfo(config, calibration_.right, device_info_);
    auto left_remap = makeRectificationRemap(config_, calibration_.left, device_info_);
    auto right_remap = makeRectificationRemap(config_, calibration_.right, device_info_);

    std::lock_guard<std::mutex> lock(mutex_);

    config_ = config;
    q_matrix_ = std::move(q_matrix);
    left_camera_info_ = std::move(left_camera_info);
    right_camera_info_ = std::move(right_camera_info);
    left_remap_ = std::move(left_remap);
    right_remap_ = std::move(right_remap);
}

crl::multisense::image::Config StereoCalibrationManger::config() const
{
    std::lock_guard<std::mutex> lock(mutex_);

    return config_;
}

cv::Matx44d StereoCalibrationManger::Q() const
{
    std::lock_guard<std::mutex> lock(mutex_);

    return q_matrix_;
}

double StereoCalibrationManger::T() const
{
    std::lock_guard<std::mutex> lock(mutex_);

    //
    // The right camera projection matrix is of the form:
    //
    // [fx,  0, cx, t * fx]
    // [ 0, fy, cy, 0     ]
    // [ 0,  0,  1, 0     ]
    //
    // divide the t * fx term by fx to get t

    return right_camera_info_.p[3] / right_camera_info_.p[0];
}

sensor_msgs::msg::CameraInfo StereoCalibrationManger::leftCameraInfo(const std::string& frame_id,
                                                                     const rclcpp::Time& stamp) const
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto camera_info = left_camera_info_;
    camera_info.header.set__frame_id(frame_id).set__stamp(stamp);

    return camera_info;
}

sensor_msgs::msg::CameraInfo StereoCalibrationManger::rightCameraInfo(const std::string& frame_id,
                                                                      const rclcpp::Time& stamp) const
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto camera_info = right_camera_info_;
    camera_info.header.set__frame_id(frame_id).set__stamp(stamp);

    return camera_info;
}

RectificationRemapT StereoCalibrationManger::leftRemap() const
{
    std::lock_guard<std::mutex> lock(mutex_);

    return left_remap_;
}

RectificationRemapT StereoCalibrationManger::rightRemap() const
{
    std::lock_guard<std::mutex> lock(mutex_);

    return right_remap_;
}

}// namespace
