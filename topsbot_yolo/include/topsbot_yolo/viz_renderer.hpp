// Copyright 2026 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#ifndef TOPSBOT_YOLO__VIZ_RENDERER_HPP_
#define TOPSBOT_YOLO__VIZ_RENDERER_HPP_

#include <opencv2/core.hpp>
#include <vector>

#include "topsbot_yolo/types.hpp"

namespace topsbot_yolo
{

class VizRenderer
{
public:
  void set_class_names(const std::vector<std::string> & names) {class_names_ = names;}

  cv::Mat draw(
    const uint8_t * src_nv12, int src_w, int src_h,
    const std::vector<Detection> & detections) const;

private:
  static cv::Mat nv12_to_bgr(const uint8_t * data, int w, int h);

  std::vector<std::string> class_names_;
};

}  // namespace topsbot_yolo

#endif  // TOPSBOT_YOLO__VIZ_RENDERER_HPP_
