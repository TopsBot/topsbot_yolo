// Copyright 2026 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#ifndef TOPSBOT_YOLO__COCO_NAMES_HPP_
#define TOPSBOT_YOLO__COCO_NAMES_HPP_

#include <cstddef>
#include <string>
#include <vector>

namespace topsbot_yolo
{

inline constexpr const char * COCO_CLASS_NAMES[] = {
  "person", "bicycle", "car", "motorcycle", "airplane", "bus", "train", "truck", "boat",
  "traffic light", "fire hydrant", "stop sign", "parking meter", "bench", "bird", "cat", "dog",
  "horse", "sheep", "cow", "elephant", "bear", "zebra", "giraffe", "backpack", "umbrella",
  "handbag", "tie", "suitcase", "frisbee", "skis", "snowboard", "sports ball", "kite",
  "baseball bat", "baseball glove", "skateboard", "surfboard", "tennis racket", "bottle",
  "wine glass", "cup", "fork", "knife", "spoon", "bowl", "banana", "apple", "sandwich",
  "orange", "broccoli", "carrot", "hot dog", "pizza", "donut", "cake", "chair", "couch",
  "potted plant", "bed", "dining table", "toilet", "tv", "laptop", "mouse", "remote",
  "keyboard", "cell phone", "microwave", "oven", "toaster", "sink", "refrigerator", "book",
  "clock", "vase", "scissors", "teddy bear", "hair drier", "toothbrush"
};

inline constexpr std::size_t COCO_CLASS_COUNT = sizeof(COCO_CLASS_NAMES) / sizeof(COCO_CLASS_NAMES[0]);

inline std::string class_name(int class_id, const std::vector<std::string> & custom_names = {})
{
  if (class_id >= 0 && class_id < static_cast<int>(custom_names.size())) {
    return custom_names[static_cast<std::size_t>(class_id)];
  }
  if (class_id >= 0 && class_id < static_cast<int>(COCO_CLASS_COUNT)) {
    return COCO_CLASS_NAMES[class_id];
  }
  return "class_" + std::to_string(class_id);
}

}  // namespace topsbot_yolo

#endif  // TOPSBOT_YOLO__COCO_NAMES_HPP_
