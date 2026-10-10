// Copyright 2026 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#include "topsbot_yolo/dmabuf_allocator.hpp"

#include "BufferAllocator/BufferAllocatorWrapper.h"

namespace topsbot_yolo
{

DmabufAllocator::~DmabufAllocator()
{
  if (allocator_) {
    FreeDmabufHeapBufferAllocator(allocator_);
    allocator_ = nullptr;
  }
}

bool DmabufAllocator::ensure_initialized()
{
  if (allocator_) {
    return true;
  }
  allocator_ = CreateDmabufHeapBufferAllocator();
  return allocator_ != nullptr;
}

BufferAllocator * DmabufAllocator::get()
{
  ensure_initialized();
  return allocator_;
}

int DmabufAllocator::alloc(const size_t size, const char * heap)
{
  if (!ensure_initialized()) {
    return -1;
  }
  return DmabufHeapAlloc(allocator_, heap, static_cast<unsigned int>(size));
}

}  // namespace topsbot_yolo
