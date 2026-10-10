// Copyright 2026 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#include "topsbot_yolo/nv12_phy_input_feeder.hpp"

#include <cstring>

#include "BufferAllocator/BufferAllocatorWrapper.h"

namespace topsbot_yolo
{

namespace
{
unsigned int dmabuf_fd_for(const TaImage & nv12)
{
  if (nv12.fd() >= 0) {
    return static_cast<unsigned int>(nv12.fd());
  }
  const int blk_id = nv12.native().frame.blk_id;
  if (blk_id > 0) {
    return static_cast<unsigned int>(blk_id);
  }
  return 0U;
}
}  // namespace

bool Nv12PhyInputFeeder::bind(
  DmabufAllocator & alloc,
  const TaImage & nv12,
  const int input_num,
  const std::vector<taconn_inout_attr_t> & /*input_attrs*/)
{
  if (input_num < 1 || nv12.mapped() == nullptr) {
    return false;
  }

  uint64_t phys_addr = nv12.phy_addr();
  size_t phys_len = 0;
  if (phys_addr == 0) {
    const unsigned int dmabuf_fd = dmabuf_fd_for(nv12);
    if (dmabuf_fd == 0U) {
      return false;
    }
    uint64_t heap_len = 0;
    if (DmabufHeapGetPhysAddr(alloc.get(), dmabuf_fd, &phys_addr, &heap_len) < 0) {
      return false;
    }
    phys_len = static_cast<size_t>(heap_len);
  } else {
    const auto & frame = nv12.native().frame;
    if (frame.phy_len > 0) {
      phys_len = static_cast<size_t>(frame.phy_len);
    } else {
      phys_len = nv12.byte_size();
    }
  }

  if (phys_addr == 0 || phys_len == 0) {
    return false;
  }

  const int width = nv12.width();
  const int height = nv12.height();
  const size_t y_size = static_cast<size_t>(width) * static_cast<size_t>(height);
  const size_t uv_size = y_size / 2;

  tensors_.assign(static_cast<std::size_t>(input_num), taconn_input_phy_t{});
  for (auto & tensor : tensors_) {
    std::memset(&tensor, 0, sizeof(tensor));
  }

  if (input_num >= 2) {
    tensors_[0].physical_table[0] = phys_addr;
    tensors_[0].size_table[0] = y_size;
    tensors_[1].physical_table[0] = phys_addr + y_size;
    tensors_[1].size_table[0] = uv_size;
  } else {
    tensors_[0].physical_table[0] = phys_addr;
    tensors_[0].size_table[0] = y_size + uv_size;
  }

  (void)phys_len;
  return true;
}

}  // namespace topsbot_yolo
