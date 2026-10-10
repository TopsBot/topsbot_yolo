# config/ 目录说明

## 内置场景 yaml（全量参数，一种模式一份）

命名：`{input_msg}_yolo{11|26}_640x480.yaml` 或 `file_yolo{11|26}.yaml`

| `params_file` | `input_msg` | 典型上游 / 用途 |
|---------------|-------------|-----------------|
| `tb_img_yolo11_640x480.yaml` | `tb_img` | usb_cam `mjpeg2nv12_640x480_zc.yaml` → `/tbmem_img`（**默认**） |
| `tb_img_yolo26_640x480.yaml` | `tb_img` | 同上，YOLO26 模型 |
| `tb_img_yolov5_640x480.yaml` | `tb_img` | 同上，YOLOv5 模型（backend 内部 NV12→BGR）|
| `tb_img_yolo11_pose_640x480.yaml` | `tb_img` | 同上，YOLO11-Pose 姿态估计（backend 内部 NV12→BGR）|
| `sensor_image_yolo11_640x480.yaml` | `sensor_image` | usb_cam `mjpeg2nv12_640x480_sensor.yaml` → `/image_raw` |
| `sensor_image_yolov5_640x480.yaml` | `sensor_image` | 同上，YOLOv5 模型 |
| `file_yolo11.yaml` | `file` | 离线 JPG，`file_cli.launch.py` |
| `file_yolo26.yaml` | `file` | 离线 JPG，YOLO26 |
| `file_yolov5.yaml` | `file` | 离线 JPG，YOLOv5 |
| `file_yolo11_pose.yaml` | `file` | 离线 JPG，YOLO11-Pose |

> YOLO 推理输入须 **NV12**。不支持 `tb_jpeg` / `sensor_compressed`（JPEG 需先硬解）。

### 与 topsbot_usb_cam 成对关系（640×480）

| usb_cam `params_file` | yolo `params_file` |
|-----------------------|-------------------|
| `mjpeg2nv12_640x480_zc.yaml` | `tb_img_yolo11_640x480.yaml` |
| `mjpeg2nv12_640x480_sensor.yaml` | `sensor_image_yolo11_640x480.yaml` |

切换 YOLO11 / YOLO26：换 `params_file` 为 `*_yolo26_*` 即可。

## 其他文件

| 文件 | 说明 |
|------|------|
| `models_manifest.yaml` | 模型下载清单（**不**作为节点参数加载） |
| `params_reference.yaml` | 参数字典（**不自动加载**） |

```bash
ls $(ros2 pkg prefix topsbot_yolo)/share/topsbot_yolo/config/*_yolo*.yaml
```

## 图像订阅参数（TOPSBOT 软性约定）

| 参数 | 说明 |
|------|------|
| `input_topic` | 订阅话题（与上游成对写在 yaml） |
| `input_msg` | `tb_img` / `sensor_image` / `file`（本节点支持子集） |
| `tb_img_profile` | **仅** `input_msg=tb_img`：`480p` / `540p` / `1080p` / `auto` |

## 参数合并顺序（后者覆盖前者）

1. `params_file` — 上表全量 yaml
2. launch 覆盖 — `input_topic`、`input_msg`、`tb_img_profile`、`frame_skip`、`image_file`、`output_image_file`（非空才写入）

```bash
ros2 launch topsbot_yolo yolo.launch.py
ros2 launch topsbot_yolo yolo.launch.py params_file:=tb_img_yolo26_640x480.yaml
ros2 launch topsbot_yolo cam_yolo_webviz_demo.launch.py
```

`cam_yolo_webviz_demo` 默认同时启动 `topsbot_usb_cam`、`topsbot_yolo`、`topsbot_webviz`（`tb_img_yolo.yaml`）；浏览器 `http://<板卡IP>:8000/`。无界面测试：`enable_webviz:=false`。

## 话题

| 方向 | 话题 | 说明 |
|------|------|------|
| 订阅 | yaml 中 `input_topic` | 默认 `/tbmem_img` 或 `/image_raw` |
| 发布 | `/detections` | `tb_det_msgs/TbPerceptionTargets`（`publish_detections: true`） |

离线 `file` 模式不发布 ROS 话题，输出 JPG 到 `output_image_file`。

### file 场景（`file_cli.launch.py`）

**默认启动**

```bash
ros2 launch topsbot_yolo file_cli.launch.py
```

默认：YOLO11（`file_yolo11.yaml`）· 输入 `share/imgs/test.jpg` · 输出 `/tmp/topsbot_yolo/out_yolo11.jpg`

**一条命令改模型 / 输入 / 输出**

```bash
ros2 launch topsbot_yolo file_cli.launch.py \
  params_file:=file_yolo26.yaml \
  image_file:=/path/to/in.jpg \
  output_image_file:=/path/to/out.jpg
```

| launch 参数 | 作用 | yaml 是否读取 |
|-------------|------|---------------|
| `params_file` | 换模型 + 默认阈值/输出 | `model_*`、`postprocess.*`、`output_image_file` |
| `image_file` | 输入 JPG | 否（launch 专用） |
| `output_image_file` | 输出 JPG | 可被 launch 覆盖 yaml |

启动后打印 `[file_cli] params_file: ...`。板端改 yaml 后需重新 `colcon build --packages-select topsbot_yolo`。
