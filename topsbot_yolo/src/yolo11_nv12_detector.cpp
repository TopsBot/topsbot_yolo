// Copyright 2026 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#include "topsbot_yolo/yolo11_nv12_detector.hpp"

#include <chrono>
#include <cmath>
#include <cfloat>
#include <vector>

#include "topsbot_yolo/coord_utils.hpp"
#include "topsbot_yolo/nms_utils.hpp"
#include "topsbot_yolo/quant_utils.hpp"

namespace topsbot_yolo
{

bool Yolo11Nv12Detector::init(const YoloDetectorConfig & cfg)
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

void Yolo11Nv12Detector::deinit()
{
  engine_.unload();
  initialized_ = false;
}

bool Yolo11Nv12Detector::detect(
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

float Yolo11Nv12Detector::softmax_with_stride(
  void * src, float * dst, const int length, const int stride,
  const int32_t zp, const float scale, const uint32_t data_format)
{
  float max_val = -FLT_MAX;
  for (int i = 0; i < length; ++i) {
    const float val = dequantize_value(src, static_cast<size_t>(i) * static_cast<size_t>(stride),
      data_format, zp, scale);
    if (val > max_val) {
      max_val = val;
    }
  }

  float sum = 0.f;
  for (int i = 0; i < length; ++i) {
    const float val = dequantize_value(src, static_cast<size_t>(i) * static_cast<size_t>(stride),
      data_format, zp, scale);
    const float exp_val = std::exp(val - max_val);
    dst[i] = exp_val;
    sum += exp_val;
  }

  float weighted_sum = 0.f;
  for (int i = 0; i < length; ++i) {
    dst[i] /= sum;
    weighted_sum += static_cast<float>(i) * dst[i];
  }
  return weighted_sum;
}

void Yolo11Nv12Detector::generate_proposals(
  const int stride, void * feat, const taconn_inout_attr_t & attr,
  const float prob_threshold, std::vector<NmsBox> & objects)
{
  const int feat_w = model_width_ / stride;
  const int feat_h = model_height_ / stride;
  constexpr int reg_max = 16;
  constexpr int cls_num = 80;
  const int channel_size = feat_h * feat_w;

  int32_t zp = 0;
  float scale = 1.f;
  get_affine_quant_params(attr, zp, scale);
  const uint32_t data_format = attr.data_format;
  const size_t elem_size = tensor_element_size(data_format);

  std::vector<float> dis_after_sm(4 * reg_max, 0.f);

  for (int h = 0; h < feat_h; ++h) {
    for (int w = 0; w < feat_w; ++w) {
      const int spatial_offset = h * feat_w + w;
      int class_index = 0;
      float class_score = -FLT_MAX;
      const size_t cls_start = static_cast<size_t>(4 * reg_max * channel_size);

      for (int s = 0; s < cls_num; ++s) {
        const size_t offset = cls_start + static_cast<size_t>(s * channel_size + spatial_offset);
        const float score = dequantize_value(feat, offset, data_format, zp, scale);
        if (score > class_score) {
          class_index = s;
          class_score = score;
        }
      }

      const float box_prob = 1.f / (1.f + std::exp(-class_score));
      if (box_prob < prob_threshold) {
        continue;
      }

      float pred_ltrb[4];
      for (int c = 0; c < 4; ++c) {
        const size_t box_start = static_cast<size_t>(c * reg_max * channel_size);
        const size_t offset = box_start + static_cast<size_t>(spatial_offset);
        auto * feat_ptr = static_cast<char *>(feat) + offset * elem_size;
        pred_ltrb[c] = softmax_with_stride(
          feat_ptr, dis_after_sm.data() + c * reg_max, reg_max, channel_size, zp, scale,
          data_format) * static_cast<float>(stride);
      }

      const float pb_cx = (static_cast<float>(w) + 0.5f) * static_cast<float>(stride);
      const float pb_cy = (static_cast<float>(h) + 0.5f) * static_cast<float>(stride);
      float x0 = pb_cx - pred_ltrb[0];
      float y0 = pb_cy - pred_ltrb[1];
      float x1 = pb_cx + pred_ltrb[2];
      float y1 = pb_cy + pred_ltrb[3];

      x0 = std::max(0.f, std::min(x0, static_cast<float>(model_width_ - 1)));
      y0 = std::max(0.f, std::min(y0, static_cast<float>(model_height_ - 1)));
      x1 = std::max(0.f, std::min(x1, static_cast<float>(model_width_ - 1)));
      y1 = std::max(0.f, std::min(y1, static_cast<float>(model_height_ - 1)));

      NmsBox obj;
      obj.left = x0;
      obj.top = y0;
      obj.width = x1 - x0;
      obj.height = y1 - y0;
      obj.class_id = class_index;
      obj.score = box_prob;
      objects.push_back(obj);
    }
  }
}

bool Yolo11Nv12Detector::postprocess(
  const PreprocessParams & params, std::vector<Detection> & out)
{
  std::vector<NmsBox> proposals;
  const int strides[3] = {8, 16, 32};
  const int head_count = std::min(engine_.output_num(), 3);

  for (int i = 0; i < head_count; ++i) {
    void * output_data = engine_.output_buffers()[i].data;
    if (!output_data) {
      return false;
    }
    generate_proposals(
      strides[i], output_data, engine_.output_attrs()[i], cfg_.postprocess.confidence_threshold,
      proposals);
  }

  nms_sorted_boxes(proposals, cfg_.postprocess.nms_threshold, cfg_.postprocess.max_detections);

  out.clear();
  out.reserve(proposals.size());
  for (const auto & obj : proposals) {
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
