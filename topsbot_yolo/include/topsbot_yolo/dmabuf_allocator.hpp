// Copyright 2026 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#ifndef TOPSBOT_YOLO__DMABUF_ALLOCATOR_HPP_
#define TOPSBOT_YOLO__DMABUF_ALLOCATOR_HPP_

#include <cstddef>

struct BufferAllocator;

namespace topsbot_yolo
{

class DmabufAllocator
{
public:
  DmabufAllocator() = default;
  ~DmabufAllocator();

  DmabufAllocator(const DmabufAllocator &) = delete;
  DmabufAllocator & operator=(const DmabufAllocator &) = delete;

  BufferAllocator * get();
  int alloc(size_t size, const char * heap = "carveout-heap-cacheable");

private:
  bool ensure_initialized();
  BufferAllocator * allocator_{nullptr};
};

}  // namespace topsbot_yolo

#endif  // TOPSBOT_YOLO__DMABUF_ALLOCATOR_HPP_
