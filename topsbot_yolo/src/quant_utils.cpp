// Copyright 2026 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#include "topsbot_yolo/quant_utils.hpp"

#include <cmath>
#include <cstring>

namespace topsbot_yolo
{

size_t tensor_element_count(const taconn_inout_attr_t & attr)
{
  size_t size = 1;
  for (unsigned int i = 0; i < attr.dim_count; ++i) {
    size *= attr.dim_size[i];
  }
  return size;
}

size_t tensor_element_size(const uint32_t data_format)
{
  switch (data_format) {
    case TACONN_DATA_FORMAT_FP32:
    case TACONN_DATA_FORMAT_INT32:
    case TACONN_DATA_FORMAT_UINT32:
      return sizeof(uint32_t);
    case TACONN_DATA_FORMAT_FP16:
    case TACONN_DATA_FORMAT_BFP16:
    case TACONN_DATA_FORMAT_INT16:
    case TACONN_DATA_FORMAT_UINT16:
      return sizeof(uint16_t);
    case TACONN_DATA_FORMAT_UINT8:
    case TACONN_DATA_FORMAT_INT8:
    case TACONN_DATA_FORMAT_CHAR:
    case TACONN_DATA_FORMAT_BOOL8:
      return sizeof(uint8_t);
    default:
      return sizeof(uint32_t);
  }
}

size_t calculate_buffer_size(const taconn_data_format_t format, const size_t element_count)
{
  return tensor_element_size(format) * element_count;
}

float dequantize_value(
  void * data, const size_t idx, const uint32_t data_format, const int32_t zp, const float scale)
{
  switch (data_format) {
    case TACONN_DATA_FORMAT_FP32:
      return static_cast<float *>(data)[idx];
    case TACONN_DATA_FORMAT_FP16: {
      const uint16_t value = static_cast<uint16_t *>(data)[idx];
      uint32_t sign = (value >> 15) & 0x1;
      uint32_t exponent = (value >> 10) & 0x1F;
      uint32_t mantissa = value & 0x3FF;
      if (exponent == 0) {
        if (mantissa == 0) {
          const uint32_t f = (sign << 31);
          float out;
          std::memcpy(&out, &f, sizeof(out));
          return out;
        }
        // 亚正规数：先把尾数左移到隐含 1（bit10），指数同步从 103 递减
        uint32_t m = mantissa;
        int32_t exponent_norm = 103;
        while ((m & 0x400U) == 0U) {
          m <<= 1;
          --exponent_norm;
        }
        const uint32_t f = (sign << 31) |
          (static_cast<uint32_t>(exponent_norm) << 23) |
          ((m & 0x3FFU) << 13);
        float out;
        std::memcpy(&out, &f, sizeof(out));
        return out;
      }
      if (exponent == 31) {
        const uint32_t f = (sign << 31) | 0x7F800000 | (mantissa << 13);
        float out;
        std::memcpy(&out, &f, sizeof(out));
        return out;
      }
      exponent += (127 - 15);
      const uint32_t f = (sign << 31) | (exponent << 23) | (mantissa << 13);
      float out;
      std::memcpy(&out, &f, sizeof(out));
      return out;
    }
    case TACONN_DATA_FORMAT_UINT8:
      return (static_cast<float>(static_cast<uint8_t *>(data)[idx]) - static_cast<float>(zp)) * scale;
    case TACONN_DATA_FORMAT_INT8:
      return (static_cast<float>(static_cast<int8_t *>(data)[idx]) - static_cast<float>(zp)) * scale;
    default:
      return 0.f;
  }
}

void get_affine_quant_params(const taconn_inout_attr_t & attr, int32_t & zp, float & scale)
{
  zp = 0;
  scale = 1.f;
  if (attr.quant_format == TACONN_QNT_TYPE_ASYMMETRIC) {
    zp = attr.quant_data.affine.tf_zero_point;
    scale = attr.quant_data.affine.tf_scale;
  }
}

}  // namespace topsbot_yolo
