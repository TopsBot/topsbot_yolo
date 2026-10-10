# Changelog

All notable changes to this package are documented here.

## 0.5.0 (2026-09-24)

### Added

- Zero-copy NPU input via phys_addr LOOKUP.
- YOLOv5 / YOLO11-pose model support and `yolo_video_cli`.
- v0.5 compliance: CI/CD pipeline and release configuration.

### Fixed

- Launch parameter handling fixes (`yolo.launch.py`, params).

## 0.3.2 (2026-07-24)

### Changed

- Release metadata aligned with topsbot_usb_cam 0.3.0 / topsbot_webviz 0.2.0 multi-camera defaults and demo stack.

## 0.3.1 (2026-07-04)

- Removed unused `imgs/test1.jpg` package test image.

## 0.3.0 (2026-06-29)

### Added

- `YoloSession::process_with_viz()` for offline file/CLI paths (NV12→BGR + draw boxes → JPG).
- Full-scene yaml configs (`tb_img_yolo*`, `sensor_image_yolo*`, `file_yolo*`).
- `file_cli.launch.py` for offline single-JPG detection.
- `cam_yolo_webviz_demo.launch.py` (usb_cam + yolo + webviz).
- `input_msg` helper; `QUICKSTART.md`, `config/README.md`, `config/params_reference.yaml`.
- Package test images under `share/imgs/`.

### Changed

- **Breaking**: online `yolo_node` no longer publishes `/yolo_viz`; primary output is `/detections` (`TbPerceptionTargets`, default on).
- Online pipeline uses `YoloSession::process()` only (no OpenCV color convert or draw per frame).
- `publish_detections` defaults to `true`; removed `viz_topic` parameter.
- Removed layered demo yamls; `cam_yolo_demo.launch.py` renamed to `cam_yolo_webviz_demo.launch.py`.
- `yolo.launch.py` supports `scene_params_file` and launch overrides.
- User-facing README rewrite (file / single-node / joint-test scenarios).

### Removed

- `config/yolo*_demo.yaml`, `yolo_demo_common.yaml`.

## 0.1.0 (2026-06-21)

### Added

- YOLO11 / YOLO26 NV12 detection node with tbmem zero-copy input.
- `yolo_file_cli` for offline JPG validation.
- Launch files: `yolo.launch.py`, `cam_yolo_demo.launch.py`.
- Model auto-download via `scripts/download_models.sh` and `ensure_models.sh`.
- Community-oriented README with apt install and demo instructions.

### Changed

- Default model directory: `~/topsbot/models`.
- Release build defaults to `-O3` when `CMAKE_BUILD_TYPE` is unset.
- Demo launch aligned with `topsbot_usb_cam` preset API.
