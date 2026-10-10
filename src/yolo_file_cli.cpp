// Copyright 2026 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#include <opencv2/imgcodecs.hpp>

#include "topsbot_yolo/image_source.hpp"
#include "topsbot_yolo/yolo_session.hpp"

namespace
{

void print_usage(const char * prog)
{
  std::cerr
    << "Usage: " << prog << " <input.jpg> <output.jpg> [options]\n"
    << "  --model-path PATH     (required) .nb model file\n"
    << "  --model-type TYPE     yolo11 | yolo26 (default: yolo11)\n"
    << "  --conf FLOAT          confidence threshold (default: 0.25)\n"
    << "  --nms FLOAT           NMS threshold, yolo11 only (default: 0.45)\n"
    << "  --max-dets INT        max detections (default: 300)\n"
    << "  --device-id INT       NPU device id (default: 0)\n";
}

std::string get_arg(int argc, char ** argv, int & i)
{
  if (i + 1 >= argc) {
    return {};
  }
  return argv[++i];
}

std::string expand_user_path(const std::string & path)
{
  if (path.empty() || path[0] != '~') {
    return path;
  }
  const char * home = std::getenv("HOME");
  if (home == nullptr || home[0] == '\0') {
    return path;
  }
  if (path.size() == 1 || path[1] == '/') {
    return std::string(home) + path.substr(1);
  }
  return path;
}

}  // namespace

int main(int argc, char ** argv)
{
  if (argc < 3) {
    print_usage(argv[0]);
    return 1;
  }

  const std::string input_path = argv[1];
  const std::string output_path = argv[2];

  topsbot_yolo::YoloDetectorConfig cfg;
  cfg.model_type = "yolo11";

  for (int i = 3; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--model-path") {
      cfg.model_path = get_arg(argc, argv, i);
    } else if (arg == "--model-type") {
      cfg.model_type = get_arg(argc, argv, i);
    } else if (arg == "--conf") {
      cfg.postprocess.confidence_threshold = std::atof(get_arg(argc, argv, i).c_str());
    } else if (arg == "--nms") {
      cfg.postprocess.nms_threshold = std::atof(get_arg(argc, argv, i).c_str());
    } else if (arg == "--max-dets") {
      cfg.postprocess.max_detections = std::atoi(get_arg(argc, argv, i).c_str());
    } else if (arg == "--device-id") {
      cfg.device_id = std::atoi(get_arg(argc, argv, i).c_str());
    } else {
      std::cerr << "Unknown option: " << arg << "\n";
      print_usage(argv[0]);
      return 1;
    }
  }

  if (cfg.model_path.empty()) {
    std::cerr << "error: --model-path is required\n";
    print_usage(argv[0]);
    return 1;
  }
  cfg.model_path = expand_user_path(cfg.model_path);
  if (!std::filesystem::exists(input_path)) {
    std::cerr << "error: input file not found: " << input_path << "\n";
    return 1;
  }

  topsbot_yolo::DmabufAllocator alloc;
  if (!alloc.get()) {
    std::cerr << "error: failed to create dmabuf allocator\n";
    return 1;
  }

  auto nv12 = topsbot_yolo::ImageSource::from_file(alloc, input_path);
  if (!nv12) {
    std::cerr << "error: failed to decode image (JPG via ta_cv jpeg dec): " << input_path << "\n";
    return 1;
  }

  topsbot_yolo::YoloSession session;
  cfg.enable_profile = true;
  if (!session.init(cfg)) {
    std::cerr << "error: failed to init YOLO session, model=" << cfg.model_path << "\n";
    return 1;
  }

  std::vector<topsbot_yolo::Detection> detections;
  topsbot_yolo::InferenceTiming timing;
  cv::Mat viz_bgr;
  if (!session.process_with_viz(alloc, nv12->image, detections, timing, viz_bgr)) {
    std::cerr << "error: detection pipeline failed\n";
    return 1;
  }

  std::filesystem::create_directories(std::filesystem::path(output_path).parent_path());
  if (!cv::imwrite(output_path, viz_bgr)) {
    std::cerr << "error: failed to write output: " << output_path << "\n";
    return 1;
  }

  const float total = timing.preprocess_ms + timing.infer_ms + timing.postprocess_ms + timing.viz_ms;
  std::cout << "Wrote " << output_path << " (" << viz_bgr.cols << "x" << viz_bgr.rows << ")\n"
            << "Detections: " << detections.size() << "\n"
            << "Timing ms: pre=" << timing.preprocess_ms
            << " infer=" << timing.infer_ms
            << " post=" << timing.postprocess_ms
            << " viz=" << timing.viz_ms
            << " total=" << total << "\n";
  return 0;
}
