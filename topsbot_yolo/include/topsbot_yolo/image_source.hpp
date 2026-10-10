// Copyright 2026 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#ifndef TOPSBOT_YOLO__IMAGE_SOURCE_HPP_
#define TOPSBOT_YOLO__IMAGE_SOURCE_HPP_

#include <optional>
#include <string>
#include <vector>

#include "sensor_msgs/msg/image.hpp"
#include "topsbot_yolo/dmabuf_allocator.hpp"
#include "topsbot_yolo/ta_image.hpp"

namespace topsbot_yolo
{

struct Nv12Buffer
{
  TaImage image;
  int width{0};
  int height{0};
};

class ImageSource
{
public:
  static std::optional<Nv12Buffer> from_file(DmabufAllocator & alloc, const std::string & path);
  static std::optional<Nv12Buffer> from_sensor_image(
    DmabufAllocator & alloc, const sensor_msgs::msg::Image & msg);
  static std::optional<Nv12Buffer> from_nv12_bytes(
    DmabufAllocator & alloc, const std::vector<uint8_t> & data, int width, int height);
  static std::optional<Nv12Buffer> from_bgr_bytes(
    DmabufAllocator & alloc, const uint8_t * bgr, int width, int height, int step);
};

}  // namespace topsbot_yolo

#endif  // TOPSBOT_YOLO__IMAGE_SOURCE_HPP_
