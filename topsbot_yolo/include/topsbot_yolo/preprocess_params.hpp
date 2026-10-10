// Copyright 2026 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#ifndef TOPSBOT_YOLO__PREPROCESS_PARAMS_HPP_
#define TOPSBOT_YOLO__PREPROCESS_PARAMS_HPP_

#include <opencv2/core.hpp>

namespace topsbot_yolo
{

struct PreprocessParams
{
  int src_width{0};
  int src_height{0};
  float ratio{1.f};
  int top{0};
  int bottom{0};
  int left{0};
  int right{0};
  int resize_width{0};
  int resize_height{0};
  cv::Size src_size;
};

}  // namespace topsbot_yolo

#endif  // TOPSBOT_YOLO__PREPROCESS_PARAMS_HPP_
