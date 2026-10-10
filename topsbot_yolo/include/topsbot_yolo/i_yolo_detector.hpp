// Copyright 2026 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#ifndef TOPSBOT_YOLO__I_YOLO_DETECTOR_HPP_
#define TOPSBOT_YOLO__I_YOLO_DETECTOR_HPP_

#include <memory>
#include <vector>

#include "topsbot_yolo/dmabuf_allocator.hpp"
#include "topsbot_yolo/ta_image.hpp"
#include "topsbot_yolo/preprocess_params.hpp"
#include "topsbot_yolo/types.hpp"

namespace topsbot_yolo
{

class IYoloDetector
{
public:
  virtual ~IYoloDetector() = default;

  virtual bool init(const YoloDetectorConfig & cfg) = 0;
  virtual void deinit() = 0;

  virtual bool detect(
    DmabufAllocator & alloc,
    const TaImage & letterbox_nv12,
    const PreprocessParams & params,
    std::vector<Detection> & out,
    InferenceTiming * timing = nullptr) = 0;

  virtual int model_width() const = 0;
  virtual int model_height() const = 0;
};

std::unique_ptr<IYoloDetector> create_yolo_detector(const YoloDetectorConfig & cfg);

}  // namespace topsbot_yolo

#endif  // TOPSBOT_YOLO__I_YOLO_DETECTOR_HPP_
