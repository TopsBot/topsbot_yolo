// Copyright 2026 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#include "topsbot_yolo/yolo26_nv12_detector.hpp"

#include <algorithm>
#include <chrono>
#include <cfloat>
#include <vector>

#include "topsbot_yolo/coord_utils.hpp"
#include "topsbot_yolo/quant_utils.hpp"

namespace topsbot_yolo
{

namespace
{
struct InternalBox
{
  float left{0.f};
  float top{0.f};
  float width{0.f};
  float height{0.f};
  int class_id{-1};
  float score{0.f};
};

void generate_from_concat(
  void * feat, const taconn_inout_attr_t & attr, float prob_threshold,
  int letterbox_w, int letterbox_h, std::vector<InternalBox> & out)
{
  constexpr int cls_num = 80;
  constexpr int box_channels = 4;
  constexpr int num_boxes = 8400;

  int32_t zp = 0;
  float scale = 1.f;
  get_affine_quant_params(attr, zp, scale);
  const uint32_t data_format = attr.data_format;

  for (int i = 0; i < num_boxes; ++i) {
    int class_index = 0;
    float class_score = -FLT_MAX;
    for (int c = 0; c < cls_num; ++c) {
      const size_t offset = static_cast<size_t>(box_channels + c) * num_boxes + static_cast<size_t>(i);
      const float score = dequantize_value(feat, offset, data_format, zp, scale);
      if (score > class_score) {
        class_score = score;
        class_index = c;
      }
    }
    if (class_score < prob_threshold) {
      continue;
    }

    float x0 = dequantize_value(feat, static_cast<size_t>(0) * num_boxes + i, data_format, zp, scale);
    float y0 = dequantize_value(feat, static_cast<size_t>(1) * num_boxes + i, data_format, zp, scale);
    float x1 = dequantize_value(feat, static_cast<size_t>(2) * num_boxes + i, data_format, zp, scale);
    float y1 = dequantize_value(feat, static_cast<size_t>(3) * num_boxes + i, data_format, zp, scale);

    x0 = std::max(0.f, std::min(x0, static_cast<float>(letterbox_w - 1)));
    y0 = std::max(0.f, std::min(y0, static_cast<float>(letterbox_h - 1)));
    x1 = std::max(0.f, std::min(x1, static_cast<float>(letterbox_w - 1)));
    y1 = std::max(0.f, std::min(y1, static_cast<float>(letterbox_h - 1)));

    InternalBox obj;
    obj.left = x0;
    obj.top = y0;
    obj.width = std::max(0.f, x1 - x0);
    obj.height = std::max(0.f, y1 - y0);
    obj.class_id = class_index;
    obj.score = class_score;
    out.push_back(obj);
  }
}

void generate_from_split(
  void * box_feat, const taconn_inout_attr_t & box_attr,
  void * cls_feat, const taconn_inout_attr_t & cls_attr,
  float prob_threshold, int letterbox_w, int letterbox_h, std::vector<InternalBox> & out)
{
  constexpr int cls_num = 80;
  constexpr int box_num = 8400;

  int32_t box_zp = 0;
  int32_t cls_zp = 0;
  float box_scale = 1.f;
  float cls_scale = 1.f;
  get_affine_quant_params(box_attr, box_zp, box_scale);
  get_affine_quant_params(cls_attr, cls_zp, cls_scale);

  for (int i = 0; i < box_num; ++i) {
    int class_index = 0;
    float class_score = -FLT_MAX;
    for (int c = 0; c < cls_num; ++c) {
      const size_t offset = static_cast<size_t>(c) * box_num + static_cast<size_t>(i);
      const float score = dequantize_value(
        cls_feat, offset, cls_attr.data_format, cls_zp, cls_scale);
      if (score > class_score) {
        class_score = score;
        class_index = c;
      }
    }
    if (class_score < prob_threshold) {
      continue;
    }

    float x0 = dequantize_value(
      box_feat, static_cast<size_t>(0) * box_num + i, box_attr.data_format, box_zp, box_scale);
    float y0 = dequantize_value(
      box_feat, static_cast<size_t>(1) * box_num + i, box_attr.data_format, box_zp, box_scale);
    float x1 = dequantize_value(
      box_feat, static_cast<size_t>(2) * box_num + i, box_attr.data_format, box_zp, box_scale);
    float y1 = dequantize_value(
      box_feat, static_cast<size_t>(3) * box_num + i, box_attr.data_format, box_zp, box_scale);

    x0 = std::max(0.f, std::min(x0, static_cast<float>(letterbox_w - 1)));
    y0 = std::max(0.f, std::min(y0, static_cast<float>(letterbox_h - 1)));
    x1 = std::max(0.f, std::min(x1, static_cast<float>(letterbox_w - 1)));
    y1 = std::max(0.f, std::min(y1, static_cast<float>(letterbox_h - 1)));

    InternalBox obj;
    obj.left = x0;
    obj.top = y0;
    obj.width = std::max(0.f, x1 - x0);
    obj.height = std::max(0.f, y1 - y0);
    obj.class_id = class_index;
    obj.score = class_score;
    out.push_back(obj);
  }
}
}  // namespace

bool Yolo26Nv12Detector::init(const YoloDetectorConfig & cfg)
{
  cfg_ = cfg;
  if (!engine_.load(cfg.model_path, cfg.device_id)) {
    return false;
  }

  if (cfg.preprocess.auto_input_size && engine_.input_num() > 0) {
    const auto & attr = engine_.input_attrs()[0];
    if (attr.dim_count >= 2) {
      model_width_ = static_cast<int>(attr.dim_size[0]);
      model_height_ = static_cast<int>(attr.dim_size[1]);
    }
  } else {
    model_width_ = cfg.preprocess.input_width;
    model_height_ = cfg.preprocess.input_height;
  }

  initialized_ = true;
  return true;
}

void Yolo26Nv12Detector::deinit()
{
  engine_.unload();
  initialized_ = false;
}

bool Yolo26Nv12Detector::detect(
  DmabufAllocator & alloc,
  const TaImage & letterbox_nv12,
  const PreprocessParams & params,
  std::vector<Detection> & out,
  InferenceTiming * timing)
{
  if (!initialized_) {
    return false;
  }

  const auto infer_start = std::chrono::high_resolution_clock::now();

  if (!feeder_.bind(alloc, letterbox_nv12, engine_.input_num(), engine_.input_attrs())) {
    return false;
  }
  if (!engine_.set_input_phy(feeder_.tensor_count(), feeder_.tensors())) {
    return false;
  }
  if (!engine_.run() || !engine_.invalidate_outputs()) {
    return false;
  }

  const auto infer_end = std::chrono::high_resolution_clock::now();
  const auto post_start = infer_end;

  if (!postprocess(params, out)) {
    return false;
  }

  const auto post_end = std::chrono::high_resolution_clock::now();
  if (timing) {
    timing->infer_ms = std::chrono::duration<float, std::milli>(infer_end - infer_start).count();
    timing->postprocess_ms = std::chrono::duration<float, std::milli>(post_end - post_start).count();
  }
  return true;
}

bool Yolo26Nv12Detector::postprocess(
  const PreprocessParams & params, std::vector<Detection> & out)
{
  std::vector<InternalBox> proposals;

  if (engine_.output_num() == 1) {
    generate_from_concat(
      engine_.output_buffers()[0].data, engine_.output_attrs()[0],
      cfg_.postprocess.confidence_threshold, model_width_, model_height_, proposals);
  } else if (engine_.output_num() == 2) {
    generate_from_split(
      engine_.output_buffers()[0].data, engine_.output_attrs()[0],
      engine_.output_buffers()[1].data, engine_.output_attrs()[1],
      cfg_.postprocess.confidence_threshold, model_width_, model_height_, proposals);
  } else {
    return false;
  }

  std::sort(proposals.begin(), proposals.end(), [](const InternalBox & a, const InternalBox & b) {
      return a.score > b.score;
    });

  const int topk = std::min(cfg_.postprocess.max_detections, static_cast<int>(proposals.size()));
  out.clear();
  out.reserve(static_cast<std::size_t>(topk));

  for (int i = 0; i < topk; ++i) {
    const auto & obj = proposals[static_cast<std::size_t>(i)];
    float x = obj.left;
    float y = obj.top;
    float w = obj.width;
    float h = obj.height;
    inverse_coordinates(x, y, w, h, params);
    if (w <= 0.f || h <= 0.f) {
      continue;
    }
    Detection det;
    det.x = x;
    det.y = y;
    det.width = w;
    det.height = h;
    det.class_id = obj.class_id;
    det.score = obj.score;
    out.push_back(det);
  }
  return true;
}

}  // namespace topsbot_yolo
