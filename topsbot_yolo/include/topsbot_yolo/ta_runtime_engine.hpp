// Copyright 2026 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#ifndef TOPSBOT_YOLO__TA_RUNTIME_ENGINE_HPP_
#define TOPSBOT_YOLO__TA_RUNTIME_ENGINE_HPP_

#include <string>
#include <vector>

#include <ta-runtime-api.h>

namespace topsbot_yolo
{

class TaRuntimeEngine
{
public:
  TaRuntimeEngine();
  ~TaRuntimeEngine();

  TaRuntimeEngine(const TaRuntimeEngine &) = delete;
  TaRuntimeEngine & operator=(const TaRuntimeEngine &) = delete;

  bool load(const std::string & model_path, int device_id);
  void unload();

  bool is_loaded() const {return loaded_;}

  int input_num() const {return input_num_;}
  int output_num() const {return output_num_;}
  const std::vector<taconn_inout_attr_t> & input_attrs() const {return input_attrs_;}
  const std::vector<taconn_inout_attr_t> & output_attrs() const {return output_attrs_;}
  taconn_buffer_t * output_buffers() {return output_buffers_;}

  bool set_input_phy(int input_num, taconn_input_phy_t * tensors);
  /// Feed a virtual-address (CPU) input buffer. Used by CPU-side preprocessors
  /// (e.g. Yolo11PoseBgrDetector) that cannot supply a dmabuf fd.
  bool set_input_buf(int index, const void * data, std::size_t size);
  bool run();
  bool invalidate_outputs();

  ta_runtime_context context() const {return context_;}

private:
  bool allocate_outputs();
  void free_outputs();

  ta_runtime_context context_{0};
  taconn_buffer_t * output_buffers_{nullptr};
  int input_num_{0};
  int output_num_{0};
  std::vector<taconn_inout_attr_t> input_attrs_;
  std::vector<taconn_inout_attr_t> output_attrs_;
  bool runtime_acquired_{false};
  bool loaded_{false};
};

}  // namespace topsbot_yolo

#endif  // TOPSBOT_YOLO__TA_RUNTIME_ENGINE_HPP_
