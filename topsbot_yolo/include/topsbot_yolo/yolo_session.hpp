// Copyright 2026 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#ifndef TOPSBOT_YOLO__YOLO_SESSION_HPP_
#define TOPSBOT_YOLO__YOLO_SESSION_HPP_

#include <memory>
#include <vector>

#include <opencv2/core.hpp>

#include "topsbot_yolo/dmabuf_allocator.hpp"
#include "topsbot_yolo/ta_image.hpp"
#include "topsbot_yolo/i_yolo_detector.hpp"
#include "topsbot_yolo/nv12_letterbox_preprocessor.hpp"
#include "topsbot_yolo/types.hpp"
#include "topsbot_yolo/viz_renderer.hpp"

namespace topsbot_yolo
{

/// Single-frame pipeline: letterbox → infer → postprocess.
/// Use process_with_viz() for offline file/CLI paths that need a boxed BGR image.
class YoloSession
{
public:
  bool init(const YoloDetectorConfig & cfg);
  void deinit();

  bool process(
    DmabufAllocator & alloc,
    const TaImage & src_nv12,
    std::vector<Detection> & detections,
    InferenceTiming & timing);

  bool process_with_viz(
    DmabufAllocator & alloc,
    const TaImage & src_nv12,
    std::vector<Detection> & detections,
    InferenceTiming & timing,
    cv::Mat & viz_bgr);

  const YoloDetectorConfig & config() const {return cfg_;}

private:
  YoloDetectorConfig cfg_;
  Nv12LetterboxPreprocessor preprocessor_;
  std::unique_ptr<IYoloDetector> detector_;
  VizRenderer viz_;
  TaImage letterbox_image_;
  bool initialized_{false};
};

}  // namespace topsbot_yolo

#endif  // TOPSBOT_YOLO__YOLO_SESSION_HPP_
