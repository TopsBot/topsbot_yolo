// Copyright 2026 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#include "topsbot_yolo/ta_runtime_engine.hpp"

#include <cstddef>
#include <cstdlib>
#include <mutex>

#include "topsbot_yolo/quant_utils.hpp"

#include <cstring>

namespace topsbot_yolo
{

namespace
{

// ta_runtime_init()/ta_runtime_deinit() are process-level and must not be
// invoked once per model context: topsbot_ai_infer creates one backend (and
// hence one TaRuntimeEngine) per NPU core, and one process can host several
// engines. Guard the pair with a shared refcount.
std::mutex g_runtime_mutex;
std::size_t g_runtime_users = 0;

bool acquire_runtime()
{
  std::lock_guard<std::mutex> lock(g_runtime_mutex);
  if (g_runtime_users == 0 && ta_runtime_init() != 0) {
    return false;
  }
  ++g_runtime_users;
  return true;
}

void release_runtime()
{
  std::lock_guard<std::mutex> lock(g_runtime_mutex);
  if (g_runtime_users == 0) {
    return;
  }
  --g_runtime_users;
  if (g_runtime_users == 0) {
    ta_runtime_deinit();
  }
}

}  // namespace

TaRuntimeEngine::TaRuntimeEngine() = default;

TaRuntimeEngine::~TaRuntimeEngine()
{
  unload();
}

bool TaRuntimeEngine::load(const std::string & model_path, const int device_id)
{
  if (loaded_) {
    return false;
  }

  if (!acquire_runtime()) {
    return false;
  }
  runtime_acquired_ = true;

  context_ = 0;
  if (ta_runtime_load_model_from_file(&context_, model_path.c_str(), device_id) != 0) {
    unload();
    return false;
  }

  taconn_input_output_num_t num{};
  if (ta_runtime_query(&context_, TACONN_QUERY_IN_OUT_NUM, &num) != 0) {
    unload();
    return false;
  }

  input_num_ = num.input_num;
  output_num_ = num.output_num;

  input_attrs_.clear();
  for (int i = 0; i < input_num_; ++i) {
    taconn_inout_attr_t attr{};
    attr.index = i;
    if (ta_runtime_query(&context_, TACONN_QUERY_INPUT_ATTR, &attr) != 0) {
      unload();
      return false;
    }
    input_attrs_.push_back(attr);
  }

  output_attrs_.clear();
  for (int i = 0; i < output_num_; ++i) {
    taconn_inout_attr_t attr{};
    attr.index = i;
    if (ta_runtime_query(&context_, TACONN_QUERY_OUTPUT_ATTR, &attr) != 0) {
      unload();
      return false;
    }
    output_attrs_.push_back(attr);
  }

  if (!allocate_outputs()) {
    unload();
    return false;
  }

  loaded_ = true;
  return true;
}

void TaRuntimeEngine::unload()
{
  free_outputs();
  if (context_ != 0) {
    ta_runtime_destroy_context(&context_);
    context_ = 0;
  }
  if (runtime_acquired_) {
    release_runtime();
    runtime_acquired_ = false;
  }
  input_num_ = 0;
  output_num_ = 0;
  input_attrs_.clear();
  output_attrs_.clear();
  loaded_ = false;
}

bool TaRuntimeEngine::allocate_outputs()
{
  output_buffers_ = static_cast<taconn_buffer_t *>(std::malloc(
      sizeof(taconn_buffer_t) * static_cast<std::size_t>(output_num_)));
  if (!output_buffers_) {
    return false;
  }
  std::memset(output_buffers_, 0, sizeof(taconn_buffer_t) * static_cast<std::size_t>(output_num_));

  for (int i = 0; i < output_num_; ++i) {
    const auto data_type = static_cast<taconn_data_format_t>(output_attrs_[i].data_format);
    const size_t buffer_size = calculate_buffer_size(data_type, tensor_element_count(output_attrs_[i]));
    if (ta_runtime_create_buffer(&context_, buffer_size, &output_buffers_[i]) != 0) {
      for (int j = 0; j < i; ++j) {
        ta_runtime_destroy_buffer(&context_, &output_buffers_[j]);
      }
      std::free(output_buffers_);
      output_buffers_ = nullptr;
      return false;
    }
  }

  if (ta_runtime_set_output(&context_, output_num_, output_buffers_) != 0) {
    free_outputs();
    return false;
  }
  return true;
}

void TaRuntimeEngine::free_outputs()
{
  if (output_buffers_ && loaded_) {
    for (int i = 0; i < output_num_; ++i) {
      ta_runtime_destroy_buffer(&context_, &output_buffers_[i]);
    }
  }
  if (output_buffers_) {
    std::free(output_buffers_);
    output_buffers_ = nullptr;
  }
}

bool TaRuntimeEngine::set_input_buf(const int index, const void * data, const std::size_t size)
{
  if (!loaded_ || !data) {
    return false;
  }
  taconn_input_t input{};
  input.index = static_cast<uint32_t>(index);
  input.data = const_cast<void *>(data);
  input.size = static_cast<uint32_t>(size);
  return ta_runtime_set_input_cva(&context_, 1, &input) == 0;
}

bool TaRuntimeEngine::set_input_phy(const int input_num, taconn_input_phy_t * tensors)
{
  if (!loaded_ || !tensors) {
    return false;
  }
  return ta_runtime_set_input_pha(&context_, input_num, tensors) == 0;
}

bool TaRuntimeEngine::run()
{
  if (!loaded_) {
    return false;
  }
  return ta_runtime_run_network(&context_) == 0;
}

bool TaRuntimeEngine::invalidate_outputs()
{
  if (!loaded_ || !output_buffers_) {
    return false;
  }
  return ta_runtime_invalidate_buffer(&context_, output_buffers_) == 0;
}

}  // namespace topsbot_yolo
