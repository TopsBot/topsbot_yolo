// Copyright 2026 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#ifndef TOPSBOT_YOLO__COORD_UTILS_HPP_
#define TOPSBOT_YOLO__COORD_UTILS_HPP_

#include <algorithm>

#include "topsbot_yolo/preprocess_params.hpp"
#include "topsbot_yolo/types.hpp"

namespace topsbot_yolo
{

inline void inverse_coordinates(
  float & x, float & y, float & w, float & h, const PreprocessParams & params)
{
  x -= static_cast<float>(params.left);
  y -= static_cast<float>(params.top);
  const float inv_ratio = 1.f / params.ratio;
  x *= inv_ratio;
  y *= inv_ratio;
  w *= inv_ratio;
  h *= inv_ratio;

  const float x1 = x + w;
  const float y1 = y + h;
  x = std::max(0.f, std::min(x, static_cast<float>(params.src_width)));
  y = std::max(0.f, std::min(y, static_cast<float>(params.src_height)));
  const float clamped_x1 = std::max(0.f, std::min(x1, static_cast<float>(params.src_width)));
  const float clamped_y1 = std::max(0.f, std::min(y1, static_cast<float>(params.src_height)));
  w = std::max(0.f, clamped_x1 - x);
  h = std::max(0.f, clamped_y1 - y);
}

}  // namespace topsbot_yolo

#endif  // TOPSBOT_YOLO__COORD_UTILS_HPP_
