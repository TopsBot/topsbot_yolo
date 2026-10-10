// Copyright 2026 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#ifndef TOPSBOT_YOLO__TA_IMAGE_HPP_
#define TOPSBOT_YOLO__TA_IMAGE_HPP_

#include <cstddef>
#include <cstdint>

#include "ta_cv_api_ext_c.h"

namespace topsbot_yolo
{

class DmabufAllocator;

class TaImage
{
public:
  TaImage();
  ~TaImage();

  TaImage(TaImage && other) noexcept;
  TaImage & operator=(TaImage && other) noexcept;

  TaImage(const TaImage &) = delete;
  TaImage & operator=(const TaImage &) = delete;

  static TaImage create_nv12(DmabufAllocator & alloc, int width, int height);
  static TaImage adopt_nv12(int fd, int width, int height, size_t byte_size, ta_image_t image);

  ta_image_t & native() {return image_;}
  const ta_image_t & native() const {return image_;}

  int width() const {return width_;}
  int height() const {return height_;}
  int fd() const {return fd_;}
  uint64_t phy_addr() const {return image_.frame.phy_addr;}
  size_t byte_size() const {return byte_size_;}
  void * mapped() const {return mapped_;}

  bool upload_nv12(const uint8_t * src, int width, int height);
  void refresh_from_native();
  void reset();

private:
  void cleanup();

  ta_image_t image_{};
  int fd_{-1};
  int width_{0};
  int height_{0};
  size_t byte_size_{0};
  void * mapped_{nullptr};
  bool owns_image_{false};
  bool self_mmaped_{false};
};

}  // namespace topsbot_yolo

#endif  // TOPSBOT_YOLO__TA_IMAGE_HPP_
