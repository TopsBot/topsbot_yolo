# topsbot_yolo

[中文文档](README_cn.md)

# Overview

`topsbot_yolo` runs YOLO object detection and pose estimation on the MES20 board, supporting **YOLO11**, **YOLO26**, **YOLOv5**, and **YOLO11-Pose**.

- **With a camera**: real-time detection; results are published for other nodes or Web UIs
- **Single image**: offline detection on a local JPG; outputs an annotated image
- **Browser preview**: use with [`topsbot_webviz`](../topsbot_webviz) to view the feed and boxes in a browser

# Bill of Materials

| # | Item | Notes |
| - | ---- | ----- |
| 1 | TOPSBOT MES20 board | NPU (`ta_runtime`) and dmabuf support |
| 2 | USB camera | For joint testing with `topsbot_usb_cam`; MJPEG 640×480 recommended |
| 3 | YOLO models | See model table below |
| 4 | `topsbot_webviz` | Web preview for joint testing (optional) |

# Build and Install

## PC (cross-compile)

Build inside the `topsbot_cc` toolchain environment to produce `install/` for the MES20:

```bash
# PC: topsbot_cc toolchain
cd topsbot_cc/topsbot_ws
./tools/build.sh -s topsbot_yolo
```

Sync `install/` to the board, or build directly on the board if a full build environment is available.

## MES20 board (install and source)

**Option A: DEB packages (recommended when available)**

```bash
# MES20
sudo apt update
sudo apt install -y tbros-humble-topsbot-ros
```

**Option B: workspace overlay**

```bash
# MES20
source /opt/tbros/humble/setup.bash
#source <install>/setup.bash
```

## Model files

Models (`.nb`) are not shipped with source. On first launch on the board, `~/topsbot/models/` is checked; missing files trigger an automatic download of `models.zip` per `config/models_manifest.yaml`, then extraction.

| Model | Default path | Input path |
| ----- | ------------ | ---------- |
| YOLO11 | `~/topsbot/models/yolo11s_nv12_float16.nb` | NV12 zero-copy |
| YOLO26 | `~/topsbot/models/yolo26s_nv12_float16.nb` | NV12 zero-copy |
| YOLOv5 | `~/topsbot/models/yolov5s_float16.nb` | BGR CPU |
| YOLO11-Pose | `~/topsbot/models/yolo11s_pose_float16.nb` | BGR CPU |

Offline or pre-installed models:

```bash
# MES20
ros2 launch topsbot_yolo file_cli.launch.py auto_download_models:=false
```

Manual download (maintainers must set `models_zip_url` in `models_manifest.yaml`):

```bash
# PC or MES20 (source tree)
./scripts/download_models.sh
```

# Usage

Scene configs are full yaml files under `config/`. Naming and paired configs are documented in [`config/README.md`](config/README.md). Quick commands: [`QUICKSTART.md`](QUICKSTART.md).

## File mode (offline single JPG)

Use when you need a quick model check without a camera. Launch: `file_cli.launch.py`.

**Default launch**

```bash
# MES20
ros2 launch topsbot_yolo file_cli.launch.py
```

| Item | Default |
|------|---------|
| Model | YOLO11 (`file_yolo11.yaml`) |
| Input | `share/topsbot_yolo/imgs/test.jpg` |
| Output | `/tmp/topsbot_yolo/out_yolo11.jpg` |

**One command for model / input / output** (include only what you need)

```bash
# MES20
ros2 launch topsbot_yolo file_cli.launch.py \
  params_file:=file_yolo26.yaml \
  image_file:=/path/to/in.jpg \
  output_image_file:=/path/to/out.jpg
```

| Launch argument | Purpose |
|-----------------|---------|
| `params_file` | Model: `file_yolo11.yaml` / `file_yolo26.yaml` / `file_yolov5.yaml` / `file_yolo11_pose.yaml` |
| `image_file` | Input JPG path |
| `output_image_file` | Output JPG path |

## Single-node mode (online inference)

Use when an upstream node already publishes images (e.g. another `topsbot_usb_cam` or a custom node). Launch: `yolo.launch.py`.

**Default launch** (subscribes `/tbmem_img`, publishes `/detections`)

```bash
# MES20
ros2 launch topsbot_yolo yolo.launch.py
```

**Common adjustments**

```bash
# MES20 — switch to YOLO26
ros2 launch topsbot_yolo yolo.launch.py params_file:=tb_img_yolo26_640x480.yaml

# MES20 — switch to YOLOv5
ros2 launch topsbot_yolo yolo.launch.py params_file:=tb_img_yolov5_640x480.yaml

# MES20 — multi-camera / different topic
ros2 launch topsbot_yolo yolo.launch.py input_topic:=/cam0/tbmem_img

# MES20 — frame skip
ros2 launch topsbot_yolo yolo.launch.py frame_skip:=2

# MES20 — sensor_msgs path (pair with usb_cam)
ros2 launch topsbot_usb_cam usb_cam.launch.py params_file:=mjpeg2nv12_640x480_sensor.yaml setup_shm:=false
ros2 launch topsbot_yolo yolo.launch.py params_file:=sensor_image_yolo11_640x480.yaml
```

| Launch argument | Purpose |
|-----------------|---------|
| `params_file` | Scene yaml (model, `input_msg`, topics, thresholds, …) |
| `input_topic` | Override subscription topic |
| `input_msg` | Override input type: `tb_img` / `sensor_image` / `file` |
| `frame_skip` | Frame skip; `0` means no skip |

List all launch arguments: `ros2 launch topsbot_yolo yolo.launch.py -s`

## Joint test mode (camera + YOLO + Web preview)

End-to-end USB camera test. Launch: `cam_yolo_webviz_demo.launch.py` (starts `topsbot_usb_cam`, `topsbot_yolo`, and `topsbot_webviz`).

**Default launch**

```bash
# MES20
source /opt/tbros/humble/setup.bash
#source <install>/setup.bash
ros2 launch topsbot_yolo cam_yolo_webviz_demo.launch.py
```

Open `http://<board-ip>:8000/` in a browser to view the NV12 feed with detection boxes overlaid.

Defaults: `usb_cam` `mjpeg2nv12_640x480_zc.yaml` + YOLO `ai_infer_yolo.yaml` + webviz `tb_img_yolo.yaml`.

**Common adjustments**

```bash
# MES20 — switch model config (edit ai_infer_yolo.yaml or pass a custom yaml)
ros2 launch topsbot_yolo cam_yolo_webviz_demo.launch.py \
  ai_infer_params_file:=/path/to/my_yolo.yaml

# MES20 — headless (no webviz)
ros2 launch topsbot_yolo cam_yolo_webviz_demo.launch.py enable_webviz:=false
```

**Verify the pipeline**

```bash
# MES20
ros2 topic hz /tbmem_img
ros2 topic hz /detections
ros2 topic echo /detections --once
```

On success, the YOLO node logs look like:

```text
[ai_infer_node]: loaded yolo backend on NPU core 0
[ai_infer_node]: unified AI node ready: backend=yolo model=yolo11 input=tb_img:/tbmem_img workers=1 strategy=least_loaded queue=2 drop=drop_oldest detections=/detections classifications=/classifications
[ai_infer_node]: AI worker core=0 healthy=true in_flight=0 assigned=0 completed=0 failed=0 dropped=0 latency_ewma=0.00ms
```

# Interface Reference

## ROS topics

| Direction | Topic | Message type | Notes |
|-----------|-------|--------------|-------|
| Subscribe | `/tbmem_img` (default) | `tb_img_msgs/TbMsg480P` etc. | `input_msg: tb_img` |
| Subscribe | `/image_raw` | `sensor_msgs/Image` | `input_msg: sensor_image` |
| Publish | `/detections` | `tb_det_msgs/TbPerceptionTargets` | Boxes in original image space |

File mode writes JPG to `output_image_file` instead of publishing ROS topics.

## Node parameters

Field dictionary: [`config/params_reference.yaml`](config/params_reference.yaml) (reference only, not auto-loaded). Grouped summary:

| Group | Key parameters | Notes |
|-------|----------------|-------|
| Input (online) | `input_msg`, `input_topic`, `tb_img_profile` | `tb_img` / `sensor_image`; `tb_img_profile` applies only when `input_msg` is `tb_img` |
| Input (file) | `image_file`, `output_image_file` | Offline JPG paths |
| Output | `publish_detections`, `detections_topic`, `frame_skip` | Detection topic and frame skip |
| Model | `model.backend`, `model.type`, `model.path` | Backend (`yolo`), model type (`yolo11`/`yolo26`/`yolov5`/`yolo11_pose`), and `.nb` path |
| Preprocess | `preprocess.letterbox`, `preprocess.input_width`, … | Letterbox and input size |
| Postprocess | `postprocess.confidence_threshold`, `nms_threshold`, `max_detections` | Thresholds and NMS |
| Debug | `enable_profile` | Print pre / infer / post timing |

Scene yaml files are full instances of the above — one file per use case:

| `params_file` | `input_msg` | Model |
|---------------|-------------|-------|
| `ai_infer_yolo.yaml` | `tb_img` | Joint test default (online, `model.type` changeable) |
| `tb_img_yolo11_640x480.yaml` | `tb_img` | YOLO11 (online default) |
| `tb_img_yolo26_640x480.yaml` | `tb_img` | YOLO26 |
| `tb_img_yolov5_640x480.yaml` | `tb_img` | YOLOv5 |
| `tb_img_yolo11_pose_640x480.yaml` | `tb_img` | YOLO11-Pose (17 keypoints) |
| `sensor_image_yolo11_640x480.yaml` | `sensor_image` | YOLO11 |
| `sensor_image_yolov5_640x480.yaml` | `sensor_image` | YOLOv5 |
| `file_yolo11.yaml` | `file` | YOLO11 (offline default) |
| `file_yolo26.yaml` | `file` | YOLO26 |
| `file_yolov5.yaml` | `file` | YOLOv5 |
| `file_yolo11_pose.yaml` | `file` | YOLO11-Pose |

Merge order: `params_file` yaml → launch overrides (`input_topic`, `frame_skip`, etc.; non-empty only). See `config/README.md`.

## Launch files

| Launch | Purpose |
|--------|---------|
| `file_cli.launch.py` | Offline file mode |
| `yolo.launch.py` | Online single node |
| `cam_yolo_webviz_demo.launch.py` | Camera + YOLO + Web joint test |

List arguments: `ros2 launch topsbot_yolo <launch> -s`

---

## License

Apache License 2.0 — see [LICENSE](./LICENSE) and [NOTICE](./NOTICE).

Contributions welcome; see [CONTRIBUTING.md](./CONTRIBUTING.md).
