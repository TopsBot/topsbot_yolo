// Copyright 2026 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#ifndef TOPSBOT_YOLO__YOLO11_POSE_BGR_DETECTOR_HPP_
#define TOPSBOT_YOLO__YOLO11_POSE_BGR_DETECTOR_HPP_

#include <cstddef>
#include <cstdlib>
#include <vector>

#include <opencv2/core.hpp>

#include "topsbot_yolo/i_yolo_detector.hpp"
#include "topsbot_yolo/ta_runtime_engine.hpp"

namespace topsbot_yolo
{

/// YOLO11-Pose detector using a standard (non-NV12) .nb model.
///
/// Input pipeline: TaImage (letterboxed NV12, produced by NV12LetterboxPreprocessor)
///   → CPU cv::cvtColor(NV12→BGR) → normalize(÷255) → NCHW float32
///   → ta_runtime_set_input (CPU→NPU copy) → run → postprocess.
///
/// Output layout (single concat tensor [56 × 8400], channel-major):
///   ch[0..3]  = cx, cy, w, h
///   ch[4]     = person score (already sigmoid'd by the model)
///   ch[5..55] = 17 keypoints × (x, y, visibility (already sigmoid'd))
///
/// Use model_type "yolo11_pose" in YoloDetectorConfig — the factory
/// selects this detector when no NV12 pose model is available.
class Yolo11PoseBgrDetector : public IYoloDetector
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
  void generate_proposals(
    void * feat,
    const taconn_inout_attr_t & attr,
    float prob_threshold,
    int num_anchors,
    std::vector<Detection> & candidates);

  bool postprocess(const PreprocessParams & params, std::vector<Detection> & out);

  static constexpr int kNumKeypoints = 17;
  /// 输出通道数: [cx,cy,w,h, score] + 17×(x,y,vis) = 56；锚点数运行时从张量元素数推导
  static constexpr int kChannels = 5 + kNumKeypoints * 3;

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
};

}  // namespace topsbot_yolo

#endif  // TOPSBOT_YOLO__YOLO11_POSE_BGR_DETECTOR_HPP_
