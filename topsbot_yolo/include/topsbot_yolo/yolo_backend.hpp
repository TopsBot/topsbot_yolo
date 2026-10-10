// Copyright 2026 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#ifndef TOPSBOT_YOLO__YOLO_BACKEND_HPP_
#define TOPSBOT_YOLO__YOLO_BACKEND_HPP_

#include <optional>
#include <string>

#include "topsbot_ai_infer/inference_backend.hpp"
#include "topsbot_yolo/dmabuf_allocator.hpp"
#include "topsbot_yolo/image_source.hpp"
#include "topsbot_yolo/yolo_session.hpp"

namespace topsbot_yolo
{

/// IInferenceBackend plugin registered as "yolo" (matches the
/// topsbot_ai_infer config `model.backend: yolo`). Wires the ROS-free
/// YoloSession pipeline (taCV letterbox -> ta_runtime infer -> NMS/postprocess)
/// into the topsbot_ai_infer framework.
///
/// topsbot_ai_infer creates one instance per NPU worker thread, so no locking
/// is needed here; the process-level ta_runtime_init/deinit refcount lives in
/// TaRuntimeEngine.
///
/// model_type dispatch (yolo11 / yolo11_pose / yolo26) is handled by
/// yolo_detector_factory inside YoloSession.
class YoloBackend : public topsbot_ai_infer::IInferenceBackend
{
public:
  YoloBackend() = default;
  ~YoloBackend() override;

  bool initialize(
    const topsbot_ai_infer::ModelConfig & cfg, std::string & error) override;

  topsbot_ai_infer::InferenceResult infer(
    const topsbot_ai_infer::InferenceFrame & frame) override;

  const char * name() const noexcept override {return "yolo";}

private:
  YoloDetectorConfig build_detector_config() const;

  /// Fill input_ with the frame's NV12 dmabuf: zero-copy when
  /// frame.phys_addr is set (publisher carveout), copy fallback otherwise.
  /// Bgr8 frames go through the CPU BGR->NV12 path. copy_ms reports the
  /// payload copy cost (0 on the zero-copy path).
  bool acquire_frame(
    const topsbot_ai_infer::InferenceFrame & frame, float & copy_ms, std::string & error);

  topsbot_ai_infer::ModelConfig config_;
  DmabufAllocator alloc_;               // one allocator per backend instance
  YoloSession session_;                 // whole pipeline; safe: one worker per instance
  std::optional<Nv12Buffer> input_;     // reused NV12 dmabuf (zero-copy and copy paths)
};

}  // namespace topsbot_yolo

#endif  // TOPSBOT_YOLO__YOLO_BACKEND_HPP_