// Copyright 2026 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#include "topsbot_yolo/yolo_backend.hpp"

#include <unistd.h>

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include <pluginlib/class_list_macros.hpp>

#include "BufferAllocator/BufferAllocatorWrapper.h"
#include "ta_cv_api_ext_c.h"

#include "topsbot_ai_infer/object_types.hpp"
#include "topsbot_yolo/coco_names.hpp"

namespace topsbot_yolo
{

using topsbot_ai_infer::InferenceFrame;
using topsbot_ai_infer::InferenceResult;
using topsbot_ai_infer::LandmarkMeta;
using topsbot_ai_infer::ModelConfig;
using topsbot_ai_infer::ObjectMeta;
using topsbot_ai_infer::ObjectType;
using topsbot_ai_infer::PixelFormat;
using topsbot_ai_infer::Point2f;
using topsbot_ai_infer::Rect;

namespace
{

using SteadyClock = std::chrono::steady_clock;

float elapsed_ms(const SteadyClock::time_point start, const SteadyClock::time_point end)
{
  return std::chrono::duration<float, std::milli>(end - start).count();
}

std::size_t nv12_bytes(const int width, const int height)
{
  return static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3U / 2U;
}

}  // namespace

YoloBackend::~YoloBackend()
{
  session_.deinit();
}

// ── initialize ────────────────────────────────────────────────────────────

bool YoloBackend::initialize(const ModelConfig & cfg, std::string & error)
{
  error.clear();
  session_.deinit();
  config_ = cfg;

  if (!session_.init(build_detector_config())) {
    error = "YoloBackend: YoloSession init failed (model_path=" + cfg.model_path + ")";
    return false;
  }
  return true;
}

// ── infer ─────────────────────────────────────────────────────────────────

InferenceResult YoloBackend::infer(const InferenceFrame & frame)
{
  InferenceResult result;
  result.backend = name();
  result.model_name = config_.model_name;
  result.device_id = config_.device_id;

  // 1) InferenceFrame -> NV12 dmabuf (zero-copy when phys_addr is set)
  float copy_ms = 0.0F;
  if (!acquire_frame(frame, copy_ms, result.error)) {
    return result;  // result.success stays false
  }
  result.timing.input_copy_ms = copy_ms;

  // 2) letterbox -> ta_runtime infer -> NMS/postprocess
  std::vector<Detection> detections;
  topsbot_yolo::InferenceTiming yolo_timing;
  if (!session_.process(alloc_, input_->image, detections, yolo_timing)) {
    result.error = "YoloBackend: session process failed";
    return result;
  }

  // 3) Detection -> ObjectMeta (+ pose keypoints as LandmarkMeta in
  //    user_metas; ai_infer's extract_landmark_points() forwards Landmark
  //    metas into TbTarget.points)
  result.frame.objects.reserve(detections.size());
  for (const auto & det : detections) {
    ObjectMeta meta;
    meta.type = ObjectType::Detection;
    meta.class_id = det.class_id;
    meta.class_name = class_name(det.class_id, config_.class_names);
    meta.confidence = det.score;
    meta.detector_bbox = Rect{det.x, det.y, det.width, det.height};
    meta.display_rect = meta.detector_bbox;

    if (!det.keypoints.empty()) {
      auto pose = std::make_shared<LandmarkMeta>();
      // LandmarkMeta.model_name is forwarded as TbPoint.type (see
      // ai_infer_node::extract_landmark_points); webviz keys its skeleton
      // connections off that string ("hand_kps"/"body_kps"/...). YOLO11-pose
      // emits COCO 17 keypoints, so tag it "coco_pose" to select the COCO
      // connection table (distinct from mediapipe's 33-point "body_kps").
      pose->model_name = "coco_pose";
      pose->points.reserve(det.keypoints.size());
      for (const auto & kp : det.keypoints) {
        pose->points.push_back(Point2f{kp.x, kp.y, kp.conf});
      }
      meta.user_metas.push_back(std::move(pose));
    }
    result.frame.objects.push_back(std::move(meta));
  }

  // 4) per-stage timing (queue/service ms are filled by ai_infer_node).
  // NOTE: accelerator_preprocess_ms is intentionally left 0 — the YOLO
  // pipeline has no separate taCV engine stage; ai_infer_node only emits
  // that perf slot when it is non-zero.
  result.timing.preprocess_ms = yolo_timing.preprocess_ms;
  result.timing.infer_ms = yolo_timing.infer_ms;
  result.timing.postprocess_ms = yolo_timing.postprocess_ms;

  // NOTE: no gesture_det-style "none" placeholder — that is a mediapipe/webviz
  // quirk (keeps /classifications_by_object stamped every frame). For YOLO an
  // empty objects list is the correct "no detections" result.
  result.success = true;
  return result;
}

// ── acquire_frame ─────────────────────────────────────────────────────────

bool YoloBackend::acquire_frame(
  const InferenceFrame & frame, float & copy_ms, std::string & error)
{
  copy_ms = 0.0F;
  const int w = frame.width;
  const int h = frame.height;
  if (w <= 0 || h <= 0 || frame.data == nullptr) {
    error = "YoloBackend: invalid frame " + std::to_string(w) + "x" +
      std::to_string(h) + " data=null";
    return false;
  }

  if (frame.format == PixelFormat::Nv12) {
    const std::size_t expected = nv12_bytes(w, h);
    if (frame.data_size < expected) {
      error = "YoloBackend: nv12 payload too small: " +
        std::to_string(frame.data_size) + " < " + std::to_string(expected);
      return false;
    }

    // Zero-copy input: resolve the publisher's phys_addr to a dmabuf view and
    // wrap it directly (mirrors yolo_node.cpp process_frame()).
    if (frame.phys_addr != 0) {
      uint64_t view_len = frame.data_size;
      const int lookup_fd =
        DmabufHeapLookupByPhys(alloc_.get(), frame.phys_addr, &view_len);
      if (lookup_fd >= 0) {
        ta_image_t native{};
        if (ta_cv_image_create_ext(h, w, FORMAT_NV12, &native, lookup_fd) == 0) {
          TaImage adopted = TaImage::adopt_nv12(lookup_fd, w, h, expected, native);
          if (adopted.mapped() != nullptr) {
            input_.emplace(Nv12Buffer{std::move(adopted), w, h});
            return true;
          }
        }
        close(lookup_fd);
      }
      // LOOKUP/create failed: fall through to the copy path.
    }

    const bool need_alloc = !input_ ||
      input_->width != w || input_->height != h ||
      input_->image.fd() < 0 || input_->image.mapped() == nullptr;
    if (need_alloc) {
      TaImage fresh = TaImage::create_nv12(alloc_, w, h);
      if (fresh.fd() < 0 || fresh.mapped() == nullptr) {
        error = "YoloBackend: create_nv12 failed for " + std::to_string(w) + "x" +
          std::to_string(h);
        return false;
      }
      input_.emplace(Nv12Buffer{std::move(fresh), w, h});
    }

    const auto t0 = SteadyClock::now();
    if (!input_->image.upload_nv12(frame.data, w, h)) {
      error = "YoloBackend: upload_nv12 failed";
      return false;
    }
    copy_ms = elapsed_ms(t0, SteadyClock::now());
    return true;
  }

  if (frame.format == PixelFormat::Bgr8) {
    const auto t0 = SteadyClock::now();
    std::optional<Nv12Buffer> converted =
      ImageSource::from_bgr_bytes(alloc_, frame.data, w, h, frame.step);
    if (!converted) {
      error = "YoloBackend: BGR->NV12 conversion failed";
      return false;
    }
    copy_ms = elapsed_ms(t0, SteadyClock::now());
    input_.emplace(std::move(*converted));
    return true;
  }

  error = "YoloBackend: unsupported pixel format (expected Nv12/Bgr8)";
  return false;
}

// ── build_detector_config ─────────────────────────────────────────────────

YoloDetectorConfig YoloBackend::build_detector_config() const
{
  YoloDetectorConfig cfg;
  cfg.model_type = config_.model_type;   // yolo11 | yolo11_pose | yolo26
  cfg.model_path = config_.model_path;
  cfg.device_id = config_.device_id;
  cfg.enable_profile = config_.enable_profile;

  cfg.preprocess.letterbox = config_.letterbox;
  cfg.preprocess.auto_input_size = config_.auto_input_size;
  cfg.preprocess.input_width = config_.input_width;
  cfg.preprocess.input_height = config_.input_height;
  cfg.preprocess.letterbox_color[0] = config_.letterbox_color[0];
  cfg.preprocess.letterbox_color[1] = config_.letterbox_color[1];
  cfg.preprocess.letterbox_color[2] = config_.letterbox_color[2];

  cfg.postprocess.confidence_threshold = config_.confidence_threshold;
  cfg.postprocess.nms_threshold = config_.nms_threshold;
  cfg.postprocess.max_detections = config_.max_detections;
  cfg.postprocess.class_names = config_.class_names;
  return cfg;
}

}  // namespace topsbot_yolo

// ── pluginlib export (must be outside the namespace) ──────────────────────
PLUGINLIB_EXPORT_CLASS(topsbot_yolo::YoloBackend, topsbot_ai_infer::IInferenceBackend)