// Copyright 2026 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#ifndef TOPSBOT_YOLO__QUANT_UTILS_HPP_
#define TOPSBOT_YOLO__QUANT_UTILS_HPP_

#include <cstddef>
#include <cstdint>

#include <ta-runtime-api.h>

namespace topsbot_yolo
{

size_t tensor_element_count(const taconn_inout_attr_t & attr);
size_t tensor_element_size(uint32_t data_format);
size_t calculate_buffer_size(taconn_data_format_t format, size_t element_count);

float dequantize_value(
  void * data, size_t idx, uint32_t data_format, int32_t zp, float scale);

void get_affine_quant_params(const taconn_inout_attr_t & attr, int32_t & zp, float & scale);

}  // namespace topsbot_yolo

#endif  // TOPSBOT_YOLO__QUANT_UTILS_HPP_
