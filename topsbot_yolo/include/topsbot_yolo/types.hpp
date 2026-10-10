// Copyright 2026 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#ifndef TOPSBOT_YOLO__TYPES_HPP_
#define TOPSBOT_YOLO__TYPES_HPP_

#include <cstdint>
#include <string>
#include <vector>

namespace topsbot_yolo
{

struct Keypoint
{
  float x{0.f};
  float y{0.f};
  float conf{0.f};
};

struct Detection
{
  float x{0.f};
  float y{0.f};
  float width{0.f};
  float height{0.f};
  int32_t class_id{-1};
  float score{0.f};
  // pose keypoints (empty for non-pose models)
  std::vector<Keypoint> keypoints;
};

struct InferenceTiming
{
  float preprocess_ms{0.f};
  float infer_ms{0.f};
  float postprocess_ms{0.f};
  float viz_ms{0.f};
};

struct PreprocessConfig
{
  bool letterbox{true};
  int letterbox_color[3]{114, 114, 114};
  bool auto_input_size{true};
  int input_width{640};
  int input_height{640};
};

struct PostprocessConfig
{
  float confidence_threshold{0.25f};
  float nms_threshold{0.45f};
  int max_detections{300};
  std::vector<std::string> class_names;
};

/// YOLOv5 anchor configuration: three heads (P3/P4/P5), each with three (w,h) pairs.
/// Only used when model_type == "yolov5". Defaults to the standard yolov5s 640×640 anchors.
struct Yolov5AnchorConfig
{
  std::array<int, 6> p3{{10, 13, 16, 30, 33, 23}};      ///< stride-8  anchors (w,h)×3
  std::array<int, 6> p4{{30, 61, 62, 45, 59, 119}};     ///< stride-16 anchors (w,h)×3
  std::array<int, 6> p5{{116, 90, 156, 198, 373, 326}};  ///< stride-32 anchors (w,h)×3
};

struct YoloDetectorConfig
{
  std::string model_type{"yolo11"};
  std::string model_path;
  int device_id{0};
  PreprocessConfig preprocess;
  PostprocessConfig postprocess;
  Yolov5AnchorConfig anchors;  ///< yolov5 only; ignored by other backends
  bool enable_profile{false};
};

}  // namespace topsbot_yolo

#endif  // TOPSBOT_YOLO__TYPES_HPP_
