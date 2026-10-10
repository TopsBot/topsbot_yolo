// Copyright 2026 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#ifndef TOPSBOT_YOLO__NMS_UTILS_HPP_
#define TOPSBOT_YOLO__NMS_UTILS_HPP_

#include <vector>

namespace topsbot_yolo
{

struct NmsBox
{
  float left{0.f};
  float top{0.f};
  float width{0.f};
  float height{0.f};
  int class_id{-1};
  float score{0.f};
};

void nms_sorted_boxes(std::vector<NmsBox> & boxes, float nms_threshold, int max_detections);

}  // namespace topsbot_yolo

#endif  // TOPSBOT_YOLO__NMS_UTILS_HPP_
