// Copyright 2026 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#include "topsbot_yolo/viz_renderer.hpp"

#include <opencv2/imgproc.hpp>

#include "topsbot_yolo/coco_names.hpp"

namespace topsbot_yolo
{

namespace
{

// COCO 17-keypoint connection table (same as model-zoo COCO_SKELETON).
constexpr int kCocoSkeleton[][2] = {
  {0, 1}, {0, 2}, {1, 2}, {1, 3}, {2, 4}, {3, 5}, {4, 6},
  {5, 7}, {7, 9}, {6, 8}, {8, 10}, {5, 6}, {5, 11}, {6, 12},
  {11, 12}, {11, 13}, {13, 15}, {12, 14}, {14, 16},
};
constexpr int kCocoSkeletonEdges =
  static_cast<int>(sizeof(kCocoSkeleton) / sizeof(kCocoSkeleton[0]));

void draw_skeleton(cv::Mat & bgr, const Detection & d)
{
  const int n = static_cast<int>(d.keypoints.size());
  if (n < 17) {
    return;
  }
  for (int e = 0; e < kCocoSkeletonEdges; ++e) {
    const int a = kCocoSkeleton[e][0];
    const int b = kCocoSkeleton[e][1];
    if (a >= n || b >= n) {
      continue;
    }
    const auto & ka = d.keypoints[a];
    const auto & kb = d.keypoints[b];
    if (ka.conf < 0.3f || kb.conf < 0.3f) {
      continue;
    }
    cv::line(
      bgr,
      cv::Point(cvRound(ka.x), cvRound(ka.y)),
      cv::Point(cvRound(kb.x), cvRound(kb.y)),
      cv::Scalar(255, 0, 255), 2);
  }
  for (const auto & kp : d.keypoints) {
    if (kp.conf < 0.3f) {
      continue;
    }
    const cv::Point p(cvRound(kp.x), cvRound(kp.y));
    cv::circle(bgr, p, 3, cv::Scalar(0, 255, 255), -1);
  }
}

}  // namespace

cv::Mat VizRenderer::nv12_to_bgr(const uint8_t * data, const int w, const int h)
{
  cv::Mat nv12(h * 3 / 2, w, CV_8UC1, const_cast<uint8_t *>(data));
  cv::Mat bgr;
  cv::cvtColor(nv12, bgr, cv::COLOR_YUV2BGR_NV12);
  return bgr;
}

cv::Mat VizRenderer::draw(
  const uint8_t * src_nv12, const int src_w, const int src_h,
  const std::vector<Detection> & detections) const
{
  cv::Mat bgr = nv12_to_bgr(src_nv12, src_w, src_h);
  for (const auto & d : detections) {
    const cv::Rect r(
      cvRound(d.x), cvRound(d.y), cvRound(d.width), cvRound(d.height));
    cv::rectangle(bgr, r, cv::Scalar(0, 255, 0), 2);
    const auto label = class_name(d.class_id, class_names_) + " " +
      std::to_string(static_cast<int>(d.score * 100.f)) + "%";
    cv::putText(
      bgr, label, cv::Point(r.x, std::max(0, r.y - 5)),
      cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1);
    if (!d.keypoints.empty()) {
      draw_skeleton(bgr, d);
    }
  }
  return bgr;
}

}  // namespace topsbot_yolo
