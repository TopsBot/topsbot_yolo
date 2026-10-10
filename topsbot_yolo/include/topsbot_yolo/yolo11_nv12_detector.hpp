// Copyright 2026 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#ifndef TOPSBOT_YOLO__YOLO11_NV12_DETECTOR_HPP_
#define TOPSBOT_YOLO__YOLO11_NV12_DETECTOR_HPP_

#include <vector>

#include "topsbot_yolo/i_yolo_detector.hpp"
#include "topsbot_yolo/nms_utils.hpp"
#include "topsbot_yolo/nv12_phy_input_feeder.hpp"
#include "topsbot_yolo/ta_runtime_engine.hpp"

namespace topsbot_yolo
{

class Yolo11Nv12Detector : public IYoloDetector
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
  bool postprocess(const PreprocessParams & params, std::vector<Detection> & out);

  void generate_proposals(
    int stride, void * feat, const taconn_inout_attr_t & attr, float prob_threshold,
    std::vector<NmsBox> & objects);

  float softmax_with_stride(
    void * src, float * dst, int length, int stride,
    int32_t zp, float scale, uint32_t data_format);

  TaRuntimeEngine engine_;
  Nv12PhyInputFeeder feeder_;
  YoloDetectorConfig cfg_;
  int model_width_{640};
  int model_height_{640};
  bool initialized_{false};
};

}  // namespace topsbot_yolo

#endif  // TOPSBOT_YOLO__YOLO11_NV12_DETECTOR_HPP_
