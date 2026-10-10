// Copyright 2026 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#ifndef TOPSBOT_YOLO__YOLOV5_DETECTOR_HPP_
#define TOPSBOT_YOLO__YOLOV5_DETECTOR_HPP_

#include <array>
#include <cstddef>
#include <cstdlib>
#include <vector>

#include <opencv2/core.hpp>

#include "topsbot_yolo/i_yolo_detector.hpp"
#include "topsbot_yolo/nms_utils.hpp"
#include "topsbot_yolo/ta_runtime_engine.hpp"

namespace topsbot_yolo
{

/// YOLOv5 anchor-based detector using a standard (non-NV12) .nb model.
///
/// Input pipeline mirrors Yolo11PoseBgrDetector: TaImage (letterboxed NV12)
///   → CPU cv::cvtColor(NV12→BGR) → normalize(÷255) → NCHW float32 (RGB order,
///   matching model-zoo YOLOv5 `transpose(2,0,1)[::-1]`) → ta_runtime_set_input
///   (CPU→NPU copy) → run → postprocess.
///
/// Output layout: three heads at stride 8/16/32, each `[3 × (5+80), feat_h, feat_w]`
///   channel-major, with per-anchor channels (x, y, w, h, obj_conf + 80 class
///   scores). Decoding is anchor-based (no DFL), mirroring model-zoo
///   generate_proposals_yolov5_generic_clip:
///     obj  = sigmoid(ch[4])
///     cls  = sigmoid(ch[5..84]), final = obj × max(cls)
///     cx   = (2·σ(dx) − 0.5 + grid_x) × stride
///     cy   = (2·σ(dy) − 0.5 + grid_y) × stride
///     w    = σ(dw)² × 4 × anchor_w
///     h    = σ(dh)² × 4 × anchor_h
///
/// Use model_type "yolov5" in YoloDetectorConfig.
class YoloV5Detector : public IYoloDetector
{
public:
  bool init(const YoloDetectorConfig & cfg) override;
  void deinit() override;

  bool detect(
    DmabufAllocator & alloc,
    const TaImage & letterbox_nv12,
    const PreprocessParams & params,
    std::vector<Detection> & out,
    InferenceTiming * timing = nullptr) override;

  int model_width() const override {return model_width_;}
  int model_height() const override {return model_height_;}

private:
  /// Decode one output head (stride 8/16/32) into NmsBox proposals.
  void generate_proposals(
    int stride,
    void * feat,
    const taconn_inout_attr_t & attr,
    float prob_threshold,
    const std::array<int, 6> & anchors,
    std::vector<NmsBox> & proposals);

  bool postprocess(const PreprocessParams & params, std::vector<Detection> & out);

  TaRuntimeEngine engine_;
  YoloDetectorConfig cfg_;
  int model_width_{640};
  int model_height_{640};
  bool initialized_{false};

  // ta_runtime_set_input_cva requires 256-byte aligned input buffers.
  // AlignedBuf wraps a posix_memalign allocation with unique_ptr-style ownership.
  struct AlignedBuf
  {
    void * ptr{nullptr};
    std::size_t bytes{0};
    ~AlignedBuf() { if (ptr) { std::free(ptr); ptr = nullptr; } }
    bool alloc(std::size_t n_bytes) {
      if (ptr) { std::free(ptr); ptr = nullptr; }
      bytes = n_bytes;
      return posix_memalign(&ptr, 256, n_bytes) == 0;
    }
    void reset() { if (ptr) { std::free(ptr); ptr = nullptr; } bytes = 0; }
  };

  AlignedBuf input_buf_;   // float32 NCHW staging (always allocated)
  bool is_fp16_{false};
  AlignedBuf fp16_buf_;    // uint16_t NCHW staging (allocated only for FP16 models)
  std::array<std::array<int, 6>, 3> anchors_;  // [P3, P4, P5] loaded from config
};

}  // namespace topsbot_yolo

#endif  // TOPSBOT_YOLO__YOLOV5_DETECTOR_HPP_