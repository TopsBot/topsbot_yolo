// Copyright 2026 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#include <unistd.h>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include "topsbot_yolo/image_source.hpp"
#include "topsbot_yolo/yolo_session.hpp"

namespace
{

void print_usage(const char * prog)
{
  std::cerr
    << "Usage: " << prog << " <input.mp4|input.gif> [output.mp4] [options]\n"
    << "Runs YOLO (pose by default) on a video/GIF frame-by-frame and writes an\n"
    << "annotated MP4, plus an animated GIF (via ffmpeg, unless --no-gif).\n\n"
    << "  --model-path PATH     .nb model file (default: ~/topsbot/models/yolo11s_pose_float16.nb)\n"
    << "  --model-type TYPE     yolo11 | yolo26 | yolov5 | yolo11_pose (default: yolo11_pose)\n"
    << "  --conf FLOAT          confidence threshold (default: 0.25)\n"
    << "  --nms FLOAT           NMS threshold (default: 0.45)\n"
    << "  --max-dets INT        max detections (default: 100)\n"
    << "  --skip INT            process every (INT+1)-th frame (default: 0)\n"
    << "  --device-id INT       NPU device id (default: 0)\n"
    << "  --gif-fps INT         GIF frame rate (default: 10)\n"
    << "  --gif-width INT       GIF target width, height scaled to keep aspect (default: 640)\n"
    << "  --no-gif              do not produce a GIF (MP4 only)\n";
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

double to_double(const std::string & s, double fallback)
{
  if (s.empty()) {
    return fallback;
  }
  char * end = nullptr;
  const double v = std::strtod(s.c_str(), &end);
  if (end == s.c_str()) {
    return fallback;
  }
  return v;
}

long to_long(const std::string & s, long fallback)
{
  if (s.empty()) {
    return fallback;
  }
  char * end = nullptr;
  const long v = std::strtol(s.c_str(), &end, 10);
  if (end == s.c_str()) {
    return fallback;
  }
  return v;
}

std::string replace_suffix(const std::string & path, const std::string & suffix)
{
  const std::filesystem::path p(path);
  return (p.parent_path() / (p.stem().string() + suffix)).string();
}

// Build a frame seq → GIF ffmpeg command. Uses a JPEG frame sequence (mjpeg is
// natively supported on-device) instead of the two-pass palette method, because
// the on-device ffmpeg lacks a PNG encoder and the palettegen/paletteuse
// filters. mpeg4 escape_directory frame stepping lets the source be a seq dir.
std::string build_gif_cmd(
  const std::string & seq_dir, const std::string & gif_path, int gif_fps, int gif_width)
{
  const std::filesystem::path seq(seq_dir);
  const std::string input_pattern = (seq / "frame_%d.jpg").string();
  const std::string vf =
    "fps=" + std::to_string(gif_fps) +
    ",scale=" + std::to_string(gif_width) + ":-2:flags=lanczos";

  // -vsync vfr + -start_number 1 so ffmpeg paces the slideshow from the
  // sequence; -framerate 25 gives frame_count hint for fps filter proportions.
  return "ffmpeg -y -loglevel error -framerate 25 -start_number 1 -i \"" +
    input_pattern + "\" -vf \"" + vf + "\" -f gif \"" + gif_path + "\"";
}

}  // namespace

int main(int argc, char ** argv)
{
  if (argc < 2) {
    print_usage(argv[0]);
    return 1;
  }

  const std::string input_path = argv[1];
  std::string output_path;
  if (argc >= 3 && argv[2][0] != '-') {
    output_path = argv[2];
  }

  topsbot_yolo::YoloDetectorConfig cfg;
  cfg.model_type = "yolo11_pose";
  cfg.model_path = "~/topsbot/models/yolo11s_pose_float16.nb";
  cfg.postprocess.max_detections = 100;

  int skip = 0;
  int device_id = 0;
  int gif_fps = 10;
  int gif_width = 640;
  bool make_gif = true;

  for (int i = (output_path.empty() ? 2 : 3); i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--model-path") {
      cfg.model_path = get_arg(argc, argv, i);
    } else if (arg == "--model-type") {
      cfg.model_type = get_arg(argc, argv, i);
    } else if (arg == "--conf") {
      cfg.postprocess.confidence_threshold =
        static_cast<float>(to_double(get_arg(argc, argv, i), 0.25));
    } else if (arg == "--nms") {
      cfg.postprocess.nms_threshold =
        static_cast<float>(to_double(get_arg(argc, argv, i), 0.45));
    } else if (arg == "--max-dets") {
      cfg.postprocess.max_detections =
        static_cast<int>(to_long(get_arg(argc, argv, i), 100));
    } else if (arg == "--skip") {
      skip = static_cast<int>(to_long(get_arg(argc, argv, i), 0));
    } else if (arg == "--device-id") {
      device_id = static_cast<int>(to_long(get_arg(argc, argv, i), 0));
    } else if (arg == "--gif-fps") {
      gif_fps = static_cast<int>(to_long(get_arg(argc, argv, i), 10));
    } else if (arg == "--gif-width") {
      gif_width = static_cast<int>(to_long(get_arg(argc, argv, i), 640));
    } else if (arg == "--no-gif") {
      make_gif = false;
    } else {
      std::cerr << "Unknown option: " << arg << "\n";
      print_usage(argv[0]);
      return 1;
    }
  }

  if (output_path.empty()) {
    output_path = replace_suffix(input_path, "_pose.mp4");
  }
  if (skip < 0) {
    skip = 0;
  }

  cfg.model_path = expand_user_path(cfg.model_path);
  cfg.device_id = device_id;
  cfg.enable_profile = true;

  if (!std::filesystem::exists(cfg.model_path)) {
    std::cerr << "error: model file not found: " << cfg.model_path << "\n"
              << "  Run scripts/ensure_models.sh (or launch with auto_download_models:=true)\n";
    return 1;
  }

  cv::VideoCapture cap(input_path);
  if (!cap.isOpened()) {
    std::cerr << "error: cannot open input: " << input_path << "\n";
    return 1;
  }

  const int width = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
  const int height = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
  double fps = cap.get(cv::CAP_PROP_FPS);
  if (!std::isfinite(fps) || fps <= 0.0) {
    fps = 25.0;
  }
  const double out_fps = fps / static_cast<double>(skip + 1);

  topsbot_yolo::DmabufAllocator alloc;
  if (!alloc.get()) {
    std::cerr << "error: failed to create dmabuf allocator\n";
    return 1;
  }

  topsbot_yolo::YoloSession session;
  if (!session.init(cfg)) {
    std::cerr << "error: failed to init YOLO session, model=" << cfg.model_path << "\n";
    return 1;
  }

  cv::VideoWriter writer(
    output_path, cv::VideoWriter::fourcc('m', 'p', '4', 'v'), out_fps,
    cv::Size(width, height));
  if (!writer.isOpened()) {
    // Some ffmpeg builds reject the odd-sized container; keep MP4 mandatory but
    // tolerate writer failures by continuing frame-only (no GIF) if it fails.
    std::cerr << "error: failed to open video writer: " << output_path << "\n";
    return 1;
  }

  // Frame-seq staging dir for GIF generation (created only when --no-gif absent).
  std::filesystem::path seq_dir;
  if (make_gif) {
    const std::string tmp = "/tmp/topsbot_frames_" + std::to_string(::getpid());
    seq_dir = tmp;
    if (std::filesystem::exists(seq_dir)) {
      std::filesystem::remove_all(seq_dir);
    }
    std::filesystem::create_directories(seq_dir);
  }

  cv::Mat frame;
  long frame_index = 0;
  long written = 0;
  while (true) {
    if (!cap.read(frame)) {
      break;
    }
    if (frame.empty()) {
      continue;
    }
    if (skip > 0 && (frame_index % (skip + 1)) != 0) {
      ++frame_index;
      continue;
    }
    ++frame_index;

    // GIF/动图帧常被 cv::VideoCapture 解码为 4 通道 BGRA（带透明通道），
    // 而 from_bgr_bytes 假设 3 通道、且 bgr_to_nv12_host 按 col*3 读像素，
    // 4 通道会导致通道错位累积、整帧内容全乱（模型输出退化）。
    // 因此先规整为 3 通道 BGR；单图/灰度图也在此兜底。
    cv::Mat bgr_frame;
    if (frame.channels() == 4) {
      cv::cvtColor(frame, bgr_frame, cv::COLOR_BGRA2BGR);
    } else if (frame.channels() == 1) {
      cv::cvtColor(frame, bgr_frame, cv::COLOR_GRAY2BGR);
    } else {
      bgr_frame = frame;  // 已是 3 通道 BGR
    }

    auto nv12 = topsbot_yolo::ImageSource::from_bgr_bytes(
      alloc, bgr_frame.data, bgr_frame.cols, bgr_frame.rows,
      static_cast<int>(bgr_frame.step));
    if (!nv12) {
      std::cerr << "error: BGR->NV12 conversion failed at frame " << frame_index << "\n";
      continue;
    }

    std::vector<topsbot_yolo::Detection> detections;
    topsbot_yolo::InferenceTiming timing;
    cv::Mat viz_bgr;
    if (!session.process_with_viz(alloc, nv12->image, detections, timing, viz_bgr)) {
      std::cerr << "error: detection pipeline failed at frame " << frame_index << "\n";
      continue;
    }
    if (viz_bgr.empty()) {
      continue;
    }
    writer.write(viz_bgr);
    ++written;

    if (!seq_dir.empty()) {
      char name[64];
      std::snprintf(name, sizeof(name), "frame_%ld.jpg", written);
      cv::imwrite((seq_dir / name).string(), viz_bgr);
    }

    if ((written % 100) == 0) {
      std::cerr << "  frame " << written << " (infer " << timing.infer_ms
                << " ms, total " << timing.preprocess_ms + timing.infer_ms +
                  timing.postprocess_ms + timing.viz_ms
                << " ms)\n";
    }
  }

  cap.release();
  writer.release();

  if (written == 0) {
    std::cerr << "error: no frames processed from " << input_path << "\n";
    return 1;
  }

  std::cout << "Wrote " << output_path << " (" << width << "x" << height
            << ", " << written << " frames @ " << out_fps << " fps)\n";

  if (make_gif && !seq_dir.empty()) {
    const std::string gif_path = replace_suffix(output_path, ".gif");
    std::filesystem::remove(gif_path);
    const std::string cmd = build_gif_cmd(seq_dir.string(), gif_path, gif_fps, gif_width);
    std::cout << "[ffmpeg] " << cmd << "\n";
    const int rc = std::system(cmd.c_str());
    std::filesystem::remove_all(seq_dir);
    if (rc == 0 && std::filesystem::exists(gif_path)) {
      std::cout << "Wrote " << gif_path << "\n";
    } else {
      std::cerr << "warning: GIF generation failed (rc=" << rc << "); MP4 kept.\n";
    }
  }

  return 0;
}