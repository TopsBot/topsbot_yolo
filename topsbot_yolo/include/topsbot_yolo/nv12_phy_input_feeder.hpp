// Copyright 2026 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#ifndef TOPSBOT_YOLO__NV12_PHY_INPUT_FEEDER_HPP_
#define TOPSBOT_YOLO__NV12_PHY_INPUT_FEEDER_HPP_

#include <cstdint>
#include <vector>

#include <ta-runtime-api.h>

#include "topsbot_yolo/dmabuf_allocator.hpp"
#include "topsbot_yolo/ta_image.hpp"

namespace topsbot_yolo
{

/// Bind NV12 dmabuf physical planes for ta_runtime_set_input_pha.
class Nv12PhyInputFeeder
{
public:
  bool bind(
    DmabufAllocator & alloc,
    const TaImage & nv12,
    int input_num,
    const std::vector<taconn_inout_attr_t> & input_attrs);

  taconn_input_phy_t * tensors() {return tensors_.data();}
  int tensor_count() const {return static_cast<int>(tensors_.size());}

private:
  std::vector<taconn_input_phy_t> tensors_;
};

}  // namespace topsbot_yolo

#endif  // TOPSBOT_YOLO__NV12_PHY_INPUT_FEEDER_HPP_
