// Copyright 2026 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#include "topsbot_yolo/yolo11_pose_bgr_detector.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <vector>

#include <opencv2/imgproc.hpp>

#include "topsbot_yolo/coord_utils.hpp"
#include "topsbot_yolo/quant_utils.hpp"

namespace topsbot_yolo
{

// ── init / deinit ────────────────────────────────────────────────────────

bool Yolo11PoseBgrDetector::init(const YoloDetectorConfig & cfg)
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

  initialized_ = true;
  return true;
}

void Yolo11PoseBgrDetector::deinit()
{
  engine_.unload();
  input_buf_.reset();
  fp16_buf_.reset();
  is_fp16_ = false;
  initialized_ = false;
}

// ── detect ───────────────────────────────────────────────────────────────

bool Yolo11PoseBgrDetector::detect(
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

  // --- CPU pre-process: NV12 → BGR → float32 NCHW ---
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

  // HWC → NCHW，并按 model-zoo YOLO11_pose 预处理的 [::-1] 反转通道：
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
// Output layout: single tensor [kChannels, num_anchors] (channel-major,
// channel * num_anchors + anchor_idx)。640×640 输入时 num_anchors=8400。
// Channels: [0..3]=cx,cy,w,h  [4]=person_score  [5..55]=17*(x,y,vis)
//
// 与 model-zoo YOLO11_pose 参考实现(yolo11_pose.cpp: generate_proposals_yolo11_generic)
// 完全一致：channel-major、score 在第 4 通道、模型内部已完成 sigmoid。
// 布局已由板端 dump 严格验证（cx 网格公式 (col+0.5)*stride 全中）。

void Yolo11PoseBgrDetector::generate_proposals(
  void * feat,
  const taconn_inout_attr_t & attr,
  float prob_threshold,
  int num_anchors,
  std::vector<Detection> & candidates)
{
  int32_t zp = 0;
  float scale = 1.f;
  get_affine_quant_params(attr, zp, scale);
  const uint32_t data_format = attr.data_format;

  const size_t na = static_cast<size_t>(num_anchors);

  for (int i = 0; i < num_anchors; ++i) {
    // score 在第 4 通道（channel-major：feat[4 * num_anchors + i]，已 sigmoid）
    const float score = dequantize_value(
      feat, static_cast<size_t>(4) * na + static_cast<size_t>(i),
      data_format, zp, scale);
    if (score < prob_threshold) {
      continue;
    }

    const float cx = dequantize_value(feat, static_cast<size_t>(0) * na + i, data_format, zp, scale);
    const float cy = dequantize_value(feat, static_cast<size_t>(1) * na + i, data_format, zp, scale);
    const float w  = dequantize_value(feat, static_cast<size_t>(2) * na + i, data_format, zp, scale);
    const float h  = dequantize_value(feat, static_cast<size_t>(3) * na + i, data_format, zp, scale);

    Detection det;
    det.x        = cx - w * 0.5f;
    det.y        = cy - h * 0.5f;
    det.width    = w;
    det.height   = h;
    det.class_id = 0;  // pose: single class (person)
    det.score    = score;
    det.keypoints.resize(kNumKeypoints);

    // keypoints: ch[5 + k*3 .. 5 + k*3 + 2] = (x, y, vis)
    for (int k = 0; k < kNumKeypoints; ++k) {
      const size_t kch = static_cast<size_t>(5 + k * 3);
      Keypoint kp;
      kp.x    = dequantize_value(feat, kch * na + i, data_format, zp, scale);
      kp.y    = dequantize_value(feat, (kch + 1) * na + i, data_format, zp, scale);
      kp.conf = dequantize_value(feat, (kch + 2) * na + i, data_format, zp, scale);
      det.keypoints[k] = kp;
    }

    candidates.push_back(std::move(det));
  }
}

// ── NMS ──────────────────────────────────────────────────────────────────

static float iou_bgr(const Detection & a, const Detection & b)
{
  const float x1 = std::max(a.x, b.x);
  const float y1 = std::max(a.y, b.y);
  const float x2 = std::min(a.x + a.width,  b.x + b.width);
  const float y2 = std::min(a.y + a.height, b.y + b.height);
  if (x2 <= x1 || y2 <= y1) {
    return 0.f;
  }
  const float inter = (x2 - x1) * (y2 - y1);
  const float ua = a.width * a.height + b.width * b.height - inter;
  return (ua > 0.f) ? inter / ua : 0.f;
}

bool Yolo11PoseBgrDetector::postprocess(
  const PreprocessParams & params, std::vector<Detection> & out)
{
  if (engine_.output_num() < 1) {
    return false;
  }
  void * output_data = engine_.output_buffers()[0].data;
  if (!output_data) {
    return false;
  }

  // 锚点数从输出张量元素数推导（640² → 8400；换输入尺寸也保持正确）
  const taconn_inout_attr_t & out_attr = engine_.output_attrs()[0];
  const std::size_t total = tensor_element_count(out_attr);

  // DEBUG: 打印输出张量信息，确认布局和格式（只打印一次）
  {
    static bool debug_printed = false;
    if (!debug_printed) {
      debug_printed = true;
      std::string dim_str;
      for (unsigned int d = 0; d < out_attr.dim_count; ++d) {
        if (d) dim_str += "x";
        dim_str += std::to_string(out_attr.dim_size[d]);
      }
      fprintf(stderr,
        "[POSE_DEBUG] output[0] dims=%s total=%zu data_format=%u num_outputs=%d\n",
        dim_str.c_str(), total, out_attr.data_format, engine_.output_num());
    }
  }

  // DEBUG: one-shot cls-score channel scan. 正常的 person score 应有明确峰值：
  // max 接近 1.0 且 >0.25 的元素个数约等于画面中的人数。若 max 极小(<0.1)
  // 或计数异常大，说明 .nb 的 cls 头编译损坏。Remove after diagnosis.
  {
    static bool layout_dumped = false;
    if (!layout_dumped) {
      layout_dumped = true;
      int32_t zp = 0;
      float scale = 1.f;
      get_affine_quant_params(out_attr, zp, scale);
      const uint32_t fmt = out_attr.data_format;
      const int na = static_cast<int>(total / static_cast<std::size_t>(kChannels));
      const size_t sna = static_cast<size_t>(na);

      float score_max = -1e30f;
      int score_gt_25 = 0;
      for (int i = 0; i < na; ++i) {
        const float s = dequantize_value(
          output_data, static_cast<size_t>(4) * sna + static_cast<size_t>(i),
          fmt, zp, scale);
        if (s > score_max) { score_max = s; }
        if (s > 0.25f) { ++score_gt_25; }
      }
      fprintf(stderr,
        "[POSE_DUMP] cls-score ch[4]: max=%.4f  n>0.25=%d\n",
        score_max, score_gt_25);
    }
  }

  if (total % static_cast<std::size_t>(kChannels) != 0) {
    return false;
  }
  const int num_anchors = static_cast<int>(total / static_cast<std::size_t>(kChannels));

  std::vector<Detection> candidates;
  candidates.reserve(256);
  generate_proposals(
    output_data, out_attr,
    cfg_.postprocess.confidence_threshold,
    num_anchors,
    candidates);

  // Sort by score descending
  std::sort(candidates.begin(), candidates.end(),
    [](const Detection & a, const Detection & b) { return a.score > b.score; });

  // NMS
  const float nms_thresh = cfg_.postprocess.nms_threshold;
  const int max_det = cfg_.postprocess.max_detections;
  std::vector<bool> suppressed(candidates.size(), false);

  out.clear();
  for (size_t i = 0; i < candidates.size(); ++i) {
    if (suppressed[i]) { continue; }

    Detection det = candidates[i];

    // NMS 必须在同一坐标系比较：det 与 candidates[j] 都处于 letterbox 空间；
    // 反 letterbox 延后到选中之后，否则带 padding 的输入（如 640×480 相机）
    // 下 IoU 会在两套坐标之间错位。
    for (size_t j = i + 1; j < candidates.size(); ++j) {
      if (!suppressed[j] && iou_bgr(det, candidates[j]) > nms_thresh) {
        suppressed[j] = true;
      }
    }

    // Inverse letterbox: bbox
    inverse_coordinates(det.x, det.y, det.width, det.height, params);
    if (det.width <= 0.f || det.height <= 0.f) { continue; }

    // Inverse letterbox: keypoints，并 clip 到源图范围
    for (auto & kp : det.keypoints) {
      kp.x = (kp.x - static_cast<float>(params.left)) / params.ratio;
      kp.y = (kp.y - static_cast<float>(params.top))  / params.ratio;
      kp.x = std::max(0.f, std::min(kp.x, static_cast<float>(params.src_width - 1)));
      kp.y = std::max(0.f, std::min(kp.y, static_cast<float>(params.src_height - 1)));
    }

    if (static_cast<int>(out.size()) >= max_det) { break; }
    out.push_back(std::move(det));
  }

  return true;
}

}  // namespace topsbot_yolo
