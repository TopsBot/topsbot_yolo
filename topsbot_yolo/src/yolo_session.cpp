// Copyright 2026 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#include "topsbot_yolo/yolo_session.hpp"

#include <chrono>

namespace topsbot_yolo
{

bool YoloSession::init(const YoloDetectorConfig & cfg)
{
  deinit();
  cfg_ = cfg;
  detector_ = create_yolo_detector(cfg_);
  if (!detector_->init(cfg_)) {
    detector_.reset();
    return false;
  }

  if (cfg_.preprocess.auto_input_size) {
    cfg_.preprocess.input_width = detector_->model_width();
    cfg_.preprocess.input_height = detector_->model_height();
  }
  preprocessor_.configure(cfg_.preprocess);
  viz_.set_class_names(cfg_.postprocess.class_names);
  initialized_ = true;
  return true;
}

void YoloSession::deinit()
{
  if (detector_) {
    detector_->deinit();
    detector_.reset();
  }
  letterbox_image_.reset();
  initialized_ = false;
}

bool YoloSession::process(
  DmabufAllocator & alloc,
  const TaImage & src_nv12,
  std::vector<Detection> & detections,
  InferenceTiming & timing)
{
  if (!initialized_ || !detector_ || !src_nv12.mapped()) {
    return false;
  }

  timing = InferenceTiming{};
  const auto pre_start = std::chrono::high_resolution_clock::now();

  topsbot_yolo::PreprocessParams params;
  if (!preprocessor_.process(alloc, src_nv12, letterbox_image_, params)) {
    return false;
  }

  const auto pre_end = std::chrono::high_resolution_clock::now();
  timing.preprocess_ms = std::chrono::duration<float, std::milli>(pre_end - pre_start).count();

  if (!detector_->detect(alloc, letterbox_image_, params, detections, &timing)) {
    return false;
  }

  return true;
}

bool YoloSession::process_with_viz(
  DmabufAllocator & alloc,
  const TaImage & src_nv12,
  std::vector<Detection> & detections,
  InferenceTiming & timing,
  cv::Mat & viz_bgr)
{
  if (!process(alloc, src_nv12, detections, timing)) {
    return false;
  }

  const auto viz_start = std::chrono::high_resolution_clock::now();
  viz_bgr = viz_.draw(
    static_cast<const uint8_t *>(src_nv12.mapped()),
    src_nv12.width(), src_nv12.height(), detections);
  timing.viz_ms = std::chrono::duration<float, std::milli>(
    std::chrono::high_resolution_clock::now() - viz_start).count();
  return true;
}

}  // namespace topsbot_yolo
