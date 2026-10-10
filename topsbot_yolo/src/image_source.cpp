// Copyright 2026 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#include "topsbot_yolo/image_source.hpp"

#include <algorithm>
#include <vector>

#include <opencv2/imgcodecs.hpp>

namespace topsbot_yolo
{

namespace
{
bool encoding_is_nv12(const std::string & encoding)
{
  return encoding == "nv12" || encoding == "yuv420";
}

bool encoding_is_bgr8(const std::string & encoding)
{
  return encoding == "bgr8";
}

void bgr_to_nv12_host(
  const uint8_t * bgr, const int width, const int height, const int step,
  std::vector<uint8_t> & nv12_out)
{
  const int y_size = width * height;
  nv12_out.resize(static_cast<size_t>(y_size) * 3U / 2U);
  uint8_t * y_plane = nv12_out.data();
  uint8_t * uv_plane = y_plane + y_size;

  for (int row = 0; row < height; ++row) {
    const uint8_t * row_bgr = bgr + row * step;
    for (int col = 0; col < width; ++col) {
      const uint8_t b = row_bgr[col * 3 + 0];
      const uint8_t g = row_bgr[col * 3 + 1];
      const uint8_t r = row_bgr[col * 3 + 2];
      const int y = ((66 * r + 129 * g + 25 * b + 128) >> 8) + 16;
      y_plane[row * width + col] = static_cast<uint8_t>(std::min(255, std::max(0, y)));
      if ((row % 2 == 0) && (col % 2 == 0)) {
        const int uv_index = (row / 2) * width + col;
        const int u = ((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128;
        const int v = ((112 * r - 94 * g - 18 * b + 128) >> 8) + 128;
        uv_plane[uv_index] = static_cast<uint8_t>(std::min(255, std::max(0, u)));
        uv_plane[uv_index + 1] = static_cast<uint8_t>(std::min(255, std::max(0, v)));
      }
    }
  }
}
}  // namespace

std::optional<Nv12Buffer> ImageSource::from_nv12_bytes(
  DmabufAllocator & alloc, const std::vector<uint8_t> & data, const int width, const int height)
{
  const size_t expected = static_cast<size_t>(width) * static_cast<size_t>(height) * 3U / 2U;
  if (data.size() < expected) {
    return std::nullopt;
  }

  Nv12Buffer out;
  out.width = width;
  out.height = height;
  out.image = TaImage::create_nv12(alloc, width, height);
  if (out.image.fd() < 0) {
    return std::nullopt;
  }
  if (!out.image.upload_nv12(data.data(), width, height)) {
    return std::nullopt;
  }
  return out;
}

std::optional<Nv12Buffer> ImageSource::from_bgr_bytes(
  DmabufAllocator & alloc, const uint8_t * bgr, const int width, const int height, const int step)
{
  if (!bgr || width <= 0 || height <= 0) {
    return std::nullopt;
  }
  std::vector<uint8_t> nv12;
  bgr_to_nv12_host(bgr, width, height, step, nv12);
  return from_nv12_bytes(alloc, nv12, width, height);
}

std::optional<Nv12Buffer> ImageSource::from_sensor_image(
  DmabufAllocator & alloc, const sensor_msgs::msg::Image & msg)
{
  const int width = static_cast<int>(msg.width);
  const int height = static_cast<int>(msg.height);
  if (width <= 0 || height <= 0) {
    return std::nullopt;
  }

  if (encoding_is_nv12(msg.encoding)) {
    return from_nv12_bytes(alloc, msg.data, width, height);
  }
  if (encoding_is_bgr8(msg.encoding)) {
    const int step = static_cast<int>(msg.step > 0 ? msg.step : width * 3);
    return from_bgr_bytes(alloc, msg.data.data(), width, height, step);
  }
  return std::nullopt;
}

std::optional<Nv12Buffer> ImageSource::from_file(DmabufAllocator & alloc, const std::string & path)
{
  const cv::Mat image = cv::imread(path, cv::IMREAD_COLOR);
  if (image.empty()) {
    return std::nullopt;
  }
  return from_bgr_bytes(alloc, image.data, image.cols, image.rows, static_cast<int>(image.step));
}

}  // namespace topsbot_yolo
