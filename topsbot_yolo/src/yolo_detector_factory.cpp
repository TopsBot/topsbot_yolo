// Copyright 2026 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#include "topsbot_yolo/i_yolo_detector.hpp"

#include <algorithm>
#include <cctype>
#include <memory>

#include "topsbot_yolo/yolo11_nv12_detector.hpp"
#include "topsbot_yolo/yolo11_pose_bgr_detector.hpp"
#include "topsbot_yolo/yolo26_nv12_detector.hpp"
#include "topsbot_yolo/yolov5_detector.hpp"

namespace topsbot_yolo
{

namespace
{
std::string to_lower(std::string s)
{
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
      return static_cast<char>(std::tolower(c));
    });
  return s;
}
}  // namespace

std::unique_ptr<IYoloDetector> create_yolo_detector(const YoloDetectorConfig & cfg)
{
  const std::string type = to_lower(cfg.model_type);
  if (type == "yolo26" || type == "26") {
    return std::make_unique<Yolo26Nv12Detector>();
  }
  if (type == "yolo11_pose" || type == "yolo11-pose") {
    return std::make_unique<Yolo11PoseBgrDetector>();
  }
  if (type == "yolov5" || type == "v5") {
    return std::make_unique<YoloV5Detector>();
  }
  return std::make_unique<Yolo11Nv12Detector>();
}

}  // namespace topsbot_yolo
