// Copyright 2026 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#include "topsbot_yolo/yolov5_detector.hpp"

#include <algorithm>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstring>
#include <vector>

#include <opencv2/imgproc.hpp>

#include "topsbot_yolo/coord_utils.hpp"
#include "topsbot_yolo/quant_utils.hpp"

namespace topsbot_yolo
{

namespace
{
/// COCO 80-class + objectness output head: values per anchor = (x,y,w,h,obj) + 80 classes.
constexpr int kClsNum = 80;
constexpr int kValuesPerAnchor = 5 + kClsNum;

/// Default YOLOv5s anchors (640×640). Used only when no anchors are provided in config.
constexpr std::array<std::array<int, 6>, 3> kDefaultAnchors{{
  {10, 13, 16, 30, 33, 23},      // P3/8
  {30, 61, 62, 45, 59, 119},     // P4/16
  {116, 90, 156, 198, 373, 326}  // P5/32
}};

inline float sigmoid(const float x)
{
  return 1.f / (1.f + std::exp(-x));
}
}  // namespace

// ── init / deinit ────────────────────────────────────────────────────────

bool YoloV5Detector::init(const YoloDetectorConfig & cfg)
{
  cfg_ = cfg;
  if (!engine_.load(cfg.model_path, cfg.device_id)) {
    return false;
  }

  if (cfg.preprocess.auto_input_size && engine_.input_num() > 0) {
    // float NCHW/NHWC/CHW 输入的 W/H 不在 dims[0..1]（那是 batch/channel），
    // 按布局解析；解析不出时沿用 config 的 input_width/height（元素总数兜底校验）。
    const auto & attr = engine_.input_attrs()[0];
    if (attr.dim_count >= 4 && attr.dim_size[1] == 3) {  // NCHW: (N,3,H,W)
      model_width_  = static_cast<int>(attr.dim_size[3]);
      model_height_ = static_cast<int>(attr.dim_size[2]);
    } else if (attr.dim_count >= 3 && attr.dim_size[attr.dim_count - 1] == 3) {  // NHWC / HWC
      model_width_  = static_cast<int>(attr.dim_size[attr.dim_count - 2]);
      model_height_ = static_cast<int>(attr.dim_size[attr.dim_count - 3]);
    } else if (attr.dim_count >= 3 && attr.dim_size[0] == 3) {  // CHW: (3,H,W)
      model_width_  = static_cast<int>(attr.dim_size[2]);
      model_height_ = static_cast<int>(attr.dim_size[1]);
    }
  } else {
    model_width_  = cfg.preprocess.input_width;
    model_height_ = cfg.preprocess.input_height;
  }

  // 一致性校验：模型输入必须是 3 通道 FP32 或 FP16 张量（元素总数 = 3×W×H）。
  if (engine_.input_num() > 0) {
    const auto & in_attr = engine_.input_attrs()[0];
    const uint32_t fmt = in_attr.data_format;
    const bool is_fp32 = (fmt == TACONN_DATA_FORMAT_FP32);
    const bool is_fp16 = (fmt == TACONN_DATA_FORMAT_FP16);
    if ((!is_fp32 && !is_fp16) ||
      tensor_element_count(in_attr) !=
      static_cast<std::size_t>(3) *
      static_cast<std::size_t>(model_width_) *
      static_cast<std::size_t>(model_height_))
    {
      engine_.unload();
      return false;
    }
    is_fp16_ = is_fp16;
  }

  // Pre-allocate 256-byte aligned staging buffers (ta_runtime_set_input_cva requirement).
  const std::size_t n_elems =
    static_cast<std::size_t>(3) *
    static_cast<std::size_t>(model_height_) *
    static_cast<std::size_t>(model_width_);
  if (!input_buf_.alloc(n_elems * sizeof(float))) {
    engine_.unload();
    return false;
  }
  if (is_fp16_) {
    if (!fp16_buf_.alloc(n_elems * sizeof(uint16_t))) {
      engine_.unload();
      return false;
    }
  }

  anchors_[0] = cfg.anchors.p3;
  anchors_[1] = cfg.anchors.p4;
  anchors_[2] = cfg.anchors.p5;

  initialized_ = true;
  return true;
}

void YoloV5Detector::deinit()
{
  engine_.unload();
  input_buf_.reset();
  fp16_buf_.reset();
  is_fp16_ = false;
  initialized_ = false;
}

// ── detect ───────────────────────────────────────────────────────────────

bool YoloV5Detector::detect(
  DmabufAllocator & /*alloc*/,
  const TaImage & letterbox_nv12,
  const PreprocessParams & params,
  std::vector<Detection> & out,
  InferenceTiming * timing)
{
  if (!initialized_) {
    return false;
  }

  const auto infer_start = std::chrono::high_resolution_clock::now();

  // --- CPU pre-process: NV12 → BGR → float32 NCHW (RGB order) ---
  const int W = letterbox_nv12.width();
  const int H = letterbox_nv12.height();
  if (W <= 0 || H <= 0 || !letterbox_nv12.mapped()) {
    return false;
  }

  // Wrap NV12 dmabuf memory as OpenCV Mat (no copy)
  cv::Mat nv12(H * 3 / 2, W, CV_8UC1,
    const_cast<void *>(letterbox_nv12.mapped()));

  cv::Mat bgr;
  cv::cvtColor(nv12, bgr, cv::COLOR_YUV2BGR_NV12);  // HxWx3 uint8

  // Normalize to [0,1] float32
  cv::Mat bgr_f32;
  bgr.convertTo(bgr_f32, CV_32F, 1.0 / 255.0);

  // HWC → NCHW，并按 model-zoo YOLOv5 预处理的 [::-1] 反转通道：
  // ch0=R, ch1=G, ch2=B（cv::split 出来的 channels[] 是 B,G,R）
  std::vector<cv::Mat> channels(3);
  cv::split(bgr_f32, channels);  // channels[0]=B, [1]=G, [2]=R

  const std::size_t plane = static_cast<std::size_t>(H) * static_cast<std::size_t>(W);
  const int dst_colors[3] = {2, 1, 0};  // NCHW 通道 → channels[] 索引
  for (int c = 0; c < 3; ++c) {
    std::memcpy(
      static_cast<float*>(input_buf_.ptr) + static_cast<std::size_t>(c) * plane,
      channels[dst_colors[c]].ptr<float>(),
      plane * sizeof(float));
  }

  // Feed into engine — convert to FP16 first if the model expects it.
  if (is_fp16_) {
    const std::size_t n = input_buf_.bytes / sizeof(float);
    auto * src = static_cast<float*>(input_buf_.ptr);
    auto * dst = static_cast<uint16_t*>(fp16_buf_.ptr);
    for (std::size_t i = 0; i < n; ++i) {
      dst[i] = static_cast<uint16_t>(cv::float16_t(src[i]).bits());
    }
    if (!engine_.set_input_buf(0, fp16_buf_.ptr, fp16_buf_.bytes)) {
      return false;
    }
  } else {
    if (!engine_.set_input_buf(
        0,
        input_buf_.ptr,
        input_buf_.bytes))
    {
      return false;
    }
  }

  if (!engine_.run() || !engine_.invalidate_outputs()) {
    return false;
  }

  const auto infer_end = std::chrono::high_resolution_clock::now();

  if (!postprocess(params, out)) {
    return false;
  }

  const auto post_end = std::chrono::high_resolution_clock::now();
  if (timing) {
    timing->infer_ms =
      std::chrono::duration<float, std::milli>(infer_end - infer_start).count();
    timing->postprocess_ms =
      std::chrono::duration<float, std::milli>(post_end - infer_end).count();
  }
  return true;
}

// ── generate_proposals ───────────────────────────────────────────────────
// 输出布局：每个 head 是一个 [3×(5+80), feat_h, feat_w] 的张量（channel-major）。
// 与 model-zoo YOLOv5 generate_proposals_yolov5_generic_clip 保持一致：
//   - 每个 anchor 占 values_per_anchor=85 个通道，排列为 (x,y,w,h,obj + 80 cls)；
//   - obj 与 cls 在模型输出里是 logits，需 sigmoid；final = σ(obj) × max σ(cls)；
//   - 中心点/宽高解码见 generate 内注释。

void YoloV5Detector::generate_proposals(
  const int stride,
  void * feat,
  const taconn_inout_attr_t & attr,
  const float prob_threshold,
  const std::array<int, 6> & anchors,
  std::vector<NmsBox> & proposals)
{
  const int feat_w = model_width_ / stride;
  const int feat_h = model_height_ / stride;
  const size_t channel_size = static_cast<size_t>(feat_h) * static_cast<size_t>(feat_w);
  const int anchor_num = static_cast<int>(anchors.size()) / 2;

  int32_t zp = 0;
  float scale = 1.f;
  get_affine_quant_params(attr, zp, scale);
  const uint32_t data_format = attr.data_format;

  for (int a = 0; a < anchor_num; ++a) {
    const size_t anchor_base = static_cast<size_t>(a) * kValuesPerAnchor * channel_size;
    const float anchor_w = static_cast<float>(anchors[static_cast<size_t>(a) * 2]);
    const float anchor_h = static_cast<float>(anchors[static_cast<size_t>(a) * 2 + 1]);

    for (int h = 0; h < feat_h; ++h) {
      for (int w = 0; w < feat_w; ++w) {
        const size_t g = static_cast<size_t>(h) * static_cast<size_t>(feat_w) +
          static_cast<size_t>(w);

        // 目标置信度（logits → sigmoid）；先低置信度裁剪，省去 cls 循环
        const float obj_conf = sigmoid(dequantize_value(
          feat, anchor_base + static_cast<size_t>(4) * channel_size + g, data_format, zp, scale));
        if (obj_conf < prob_threshold) {
          continue;
        }

        // 类别分数（logits → sigmoid），取最大
        int class_id = 0;
        float max_cls_score = -FLT_MAX;
        for (int c = 0; c < kClsNum; ++c) {
          const float cls_score = sigmoid(dequantize_value(
            feat, anchor_base + static_cast<size_t>(5 + c) * channel_size + g,
            data_format, zp, scale));
          if (cls_score > max_cls_score) {
            max_cls_score = cls_score;
            class_id = c;
          }
        }

        const float final_score = obj_conf * max_cls_score;
        if (final_score < prob_threshold) {
          continue;
        }

        // 解码 box：x/y/w/h 均为 logits
        const float dx = sigmoid(dequantize_value(
          feat, anchor_base + static_cast<size_t>(0) * channel_size + g, data_format, zp, scale));
        const float dy = sigmoid(dequantize_value(
          feat, anchor_base + static_cast<size_t>(1) * channel_size + g, data_format, zp, scale));
        const float dw = sigmoid(dequantize_value(
          feat, anchor_base + static_cast<size_t>(2) * channel_size + g, data_format, zp, scale));
        const float dh = sigmoid(dequantize_value(
          feat, anchor_base + static_cast<size_t>(3) * channel_size + g, data_format, zp, scale));

        const float pred_cx = (dx * 2.0f - 0.5f + static_cast<float>(w)) * static_cast<float>(stride);
        const float pred_cy = (dy * 2.0f - 0.5f + static_cast<float>(h)) * static_cast<float>(stride);
        const float pred_w = dw * dw * 4.0f * anchor_w;
        const float pred_h = dh * dh * 4.0f * anchor_h;

        float x0 = pred_cx - pred_w * 0.5f;
        float y0 = pred_cy - pred_h * 0.5f;
        float x1 = pred_cx + pred_w * 0.5f;
        float y1 = pred_cy + pred_h * 0.5f;

        x0 = std::max(0.f, std::min(x0, static_cast<float>(model_width_ - 1)));
        y0 = std::max(0.f, std::min(y0, static_cast<float>(model_height_ - 1)));
        x1 = std::max(0.f, std::min(x1, static_cast<float>(model_width_ - 1)));
        y1 = std::max(0.f, std::min(y1, static_cast<float>(model_height_ - 1)));

        NmsBox obj;
        obj.left = x0;
        obj.top = y0;
        obj.width = std::max(0.f, x1 - x0);
        obj.height = std::max(0.f, y1 - y0);
        obj.class_id = class_id;
        obj.score = final_score;
        proposals.push_back(obj);
      }
    }
  }
}

// ── postprocess ──────────────────────────────────────────────────────────

bool YoloV5Detector::postprocess(
  const PreprocessParams & params, std::vector<Detection> & out)
{
  if (engine_.output_num() < 3) {
    return false;
  }

  std::vector<NmsBox> proposals;
  proposals.reserve(256);

  const int strides[3] = {8, 16, 32};
  for (int i = 0; i < 3; ++i) {
    void * output_data = engine_.output_buffers()[static_cast<size_t>(i)].data;
    if (!output_data) {
      return false;
    }
    generate_proposals(
      strides[i], output_data, engine_.output_attrs()[static_cast<size_t>(i)],
      cfg_.postprocess.confidence_threshold, anchors_[static_cast<size_t>(i)], proposals);
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