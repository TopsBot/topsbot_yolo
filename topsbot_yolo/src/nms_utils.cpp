// Copyright 2026 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#include "topsbot_yolo/nms_utils.hpp"

#include <algorithm>
#include <map>

namespace topsbot_yolo
{

namespace
{
float intersection_area(const NmsBox & a, const NmsBox & b)
{
  const float x1 = std::max(a.left, b.left);
  const float y1 = std::max(a.top, b.top);
  const float x2 = std::min(a.left + a.width, b.left + b.width);
  const float y2 = std::min(a.top + a.height, b.top + b.height);
  if (x2 <= x1 || y2 <= y1) {
    return 0.f;
  }
  return (x2 - x1) * (y2 - y1);
}
}  // namespace

void nms_sorted_boxes(
  std::vector<NmsBox> & boxes, const float nms_threshold, const int max_detections)
{
  std::sort(boxes.begin(), boxes.end(), [](const NmsBox & a, const NmsBox & b) {
      return a.score > b.score;
    });

  std::vector<NmsBox> kept;
  kept.reserve(static_cast<std::size_t>(max_detections));

  std::map<int, std::vector<int>> class_indices;
  for (int i = 0; i < static_cast<int>(boxes.size()); ++i) {
    class_indices[boxes[static_cast<std::size_t>(i)].class_id].push_back(i);
  }

  for (auto & pair : class_indices) {
    const auto & indices = pair.second;
    std::vector<int> picked;
    const int n = static_cast<int>(indices.size());
    std::vector<float> areas(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
      const auto & box = boxes[static_cast<std::size_t>(indices[static_cast<std::size_t>(i)])];
      areas[static_cast<std::size_t>(i)] = box.width * box.height;
    }

    for (int i = 0; i < n; ++i) {
      const int idx_i = indices[static_cast<std::size_t>(i)];
      const NmsBox & a = boxes[static_cast<std::size_t>(idx_i)];
      bool keep = true;
      for (const int idx_j : picked) {
        const NmsBox & b = boxes[static_cast<std::size_t>(idx_j)];
        const float inter = intersection_area(a, b);
        const float union_area = areas[static_cast<std::size_t>(i)] +
          b.width * b.height - inter;
        if (union_area > 0.f && inter / union_area > nms_threshold) {
          keep = false;
          break;
        }
      }
      if (keep) {
        picked.push_back(idx_i);
        if (static_cast<int>(kept.size()) >= max_detections) {
          break;
        }
        kept.push_back(a);
      }
    }
    if (static_cast<int>(kept.size()) >= max_detections) {
      break;
    }
  }

  boxes = std::move(kept);
}

}  // namespace topsbot_yolo
