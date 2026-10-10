// Copyright 2026 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#ifndef TOPSBOT_YOLO__NV12_LETTERBOX_PREPROCESSOR_HPP_
#define TOPSBOT_YOLO__NV12_LETTERBOX_PREPROCESSOR_HPP_

#include "topsbot_yolo/dmabuf_allocator.hpp"
#include "topsbot_yolo/ta_image.hpp"
#include "topsbot_yolo/preprocess_params.hpp"
#include "topsbot_yolo/types.hpp"

namespace topsbot_yolo
{

/// taCV resize + CPU letterbox fill (114 gray), aligned with taco-stream.
class Nv12LetterboxPreprocessor
{
public:
  void configure(const PreprocessConfig & cfg);

  bool process(
    DmabufAllocator & alloc,
    const TaImage & src_nv12,
    TaImage & letterbox_nv12,
    PreprocessParams & params);

  int model_width() const {return model_width_;}
  int model_height() const {return model_height_;}

private:
  // CPU 缩放 NV12（Y 平面 + 交错的 UV 平面分别线性插值）。
  // ta_cv 硬件缩放器对「放大」会判 Resize_attr illegal 并回退 CPU，
  // 且每帧刷 3 行 ERROR/WARN 日志；放大场景直接走此路径，全静默。
  bool resize_nv12_cpu(
    const TaImage & src, TaImage & dst, int dst_w, int dst_h);

  bool letterbox_fill_cpu(
    const TaImage & resized,
    TaImage & letterbox,
    const PreprocessParams & params);

  PreprocessConfig cfg_;
  int model_width_{640};
  int model_height_{640};
};

}  // namespace topsbot_yolo

#endif  // TOPSBOT_YOLO__NV12_LETTERBOX_PREPROCESSOR_HPP_
