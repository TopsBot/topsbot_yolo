# topsbot_yolo

[English](README.md)

# 功能介绍

`topsbot_yolo` 在 MES20 开发板上做 YOLO 目标检测与姿态估计，支持 **YOLO11**、**YOLO26**、**YOLOv5** 与 **YOLO11-Pose**。

- **接相机**：实时识别画面中的物体，输出检测结果
- **单张图片**：对本地 JPG 做离线检测，生成带框图片
- **浏览器预览**：配合 [`topsbot_webviz`](../topsbot_webviz) 在网页上查看画面与检测框

# 物料清单

| 序号 | 名称 | 说明 |
| ---- | ---- | ---- |
| 1 | TOPSBOT MES20 板卡 | NPU（`ta_runtime`）与 dmabuf 环境 |
| 2 | USB 摄像头 | 联合测试时配合 `topsbot_usb_cam`，建议 MJPEG 640×480 |
| 3 | YOLO 模型 | 见下方模型文件表 |
| 4 | `topsbot_webviz` | 联合测试时的 Web 预览（可选） |

# 编译与安装

## PC 端（交叉编译）

在 `topsbot_cc` 工具链环境中编译，生成供 MES20 安装的 `install/`：

```bash
# PC：topsbot_cc 工具链环境
cd topsbot_cc/topsbot_ws
./tools/build.sh -s topsbot_yolo
```

将 `install/` 同步到板卡，或直接在板端工作空间中编译（若板端具备完整编译环境）。

## MES20 板端（安装与加载）

**方式 A：DEB 包（推荐，源可用时）**

```bash
# MES20
sudo apt update
sudo apt install -y tbros-humble-topsbot-ros
```

**方式 B：加载工作空间**

```bash
# MES20
source /opt/tbros/humble/setup.bash
#source <install>/setup.bash
```

## 模型文件

模型（`.nb`）不随源码分发。首次在板端启动时，launch 会检查 `~/topsbot/models/`，缺失则按 `config/models_manifest.yaml` 自动下载 `models.zip` 并解压。

| 模型 | 默认路径 | 输入路径 |
| ---- | -------- | -------- |
| YOLO11 | `~/topsbot/models/yolo11s_nv12_float16.nb` | NV12 零拷贝 |
| YOLO26 | `~/topsbot/models/yolo26s_nv12_float16.nb` | NV12 零拷贝 |
| YOLOv5 | `~/topsbot/models/yolov5s_float16.nb` | BGR CPU |
| YOLO11-Pose | `~/topsbot/models/yolo11s_pose_float16.nb` | BGR CPU |

离线环境或已手动放置模型时：

```bash
# MES20
ros2 launch topsbot_yolo file_cli.launch.py auto_download_models:=false
```

手动下载脚本（维护者需先配置 `models_manifest.yaml` 中的 `models_zip_url`）：

```bash
# PC 或 MES20（源码树）
./scripts/download_models.sh
```

# 使用方式

场景配置为全量 yaml，位于 `config/`。命名与成对关系见 [`config/README.md`](config/README.md)。快速命令见 [`QUICKSTART.md`](QUICKSTART.md)。

## file 模式（离线单张 JPG）

适用：不接相机，快速验证模型与画框。Launch：`file_cli.launch.py`。

**默认启动**

```bash
# MES20
ros2 launch topsbot_yolo file_cli.launch.py
```

| 项 | 默认值 |
|----|--------|
| 模型 | YOLO11（`file_yolo11.yaml`） |
| 输入 | `share/topsbot_yolo/imgs/test.jpg` |
| 输出 | `/tmp/topsbot_yolo/out_yolo11.jpg` |

**一条命令改模型 / 输入 / 输出**（只写需要改的即可）

```bash
# MES20
ros2 launch topsbot_yolo file_cli.launch.py \
  params_file:=file_yolo26.yaml \
  image_file:=/path/to/in.jpg \
  output_image_file:=/path/to/out.jpg
```

| launch 参数 | 作用 |
|-------------|------|
| `params_file` | 换模型：`file_yolo11.yaml` / `file_yolo26.yaml` / `file_yolov5.yaml` / `file_yolo11_pose.yaml` |
| `image_file` | 输入 JPG 路径 |
| `output_image_file` | 输出 JPG 路径 |

## 单节点模式（在线推理）

适用：上游已发布图像话题（如另一路 `topsbot_usb_cam` 或自定义节点）。Launch：`yolo.launch.py`。

**默认启动**（订阅 `/tbmem_img`，发布 `/detections`）

```bash
# MES20
ros2 launch topsbot_yolo yolo.launch.py
```

**常用调整**

```bash
# MES20 — 切换 YOLO26
ros2 launch topsbot_yolo yolo.launch.py params_file:=tb_img_yolo26_640x480.yaml

# MES20 — 切换 YOLOv5
ros2 launch topsbot_yolo yolo.launch.py params_file:=tb_img_yolov5_640x480.yaml

# MES20 — 多相机 / 换订阅话题
ros2 launch topsbot_yolo yolo.launch.py input_topic:=/cam0/tbmem_img

# MES20 — 跳帧降负载
ros2 launch topsbot_yolo yolo.launch.py frame_skip:=2

# MES20 — sensor_msgs 路径（须与 usb_cam 成对切换）
ros2 launch topsbot_usb_cam usb_cam.launch.py params_file:=mjpeg2nv12_640x480_sensor.yaml setup_shm:=false
ros2 launch topsbot_yolo yolo.launch.py params_file:=sensor_image_yolo11_640x480.yaml
```

| launch 参数 | 作用 |
|-------------|------|
| `params_file` | 场景 yaml（模型、`input_msg`、话题、阈值等） |
| `input_topic` | 覆盖订阅话题 |
| `input_msg` | 覆盖输入类型：`tb_img` / `sensor_image` / `file` |
| `frame_skip` | 跳帧，0 表示不跳 |

查看全部 launch 参数：`ros2 launch topsbot_yolo yolo.launch.py -s`

## 联合测试模式（相机 + YOLO + Web 预览）

适用：USB 相机端到端联调。Launch：`cam_yolo_webviz_demo.launch.py`（一次启动 `topsbot_usb_cam`、`topsbot_yolo`、`topsbot_webviz`）。

**默认启动**

```bash
# MES20
source /opt/tbros/humble/setup.bash
#source <install>/setup.bash
ros2 launch topsbot_yolo cam_yolo_webviz_demo.launch.py
```

浏览器访问 `http://<板卡IP>:8000/` 查看 NV12 画面与检测框叠加。

默认配置：`usb_cam` `mjpeg2nv12_640x480_zc.yaml` + YOLO `ai_infer_yolo.yaml` + webviz `tb_img_yolo.yaml`。

**常用调整**

```bash
# MES20 — 切换模型配置（改 ai_infer_yolo.yaml 或传入自定义 yaml）
ros2 launch topsbot_yolo cam_yolo_webviz_demo.launch.py \
  ai_infer_params_file:=/path/to/my_yolo.yaml

# MES20 — 仅推理，不要 Web 预览
ros2 launch topsbot_yolo cam_yolo_webviz_demo.launch.py enable_webviz:=false
```

**确认链路正常**

```bash
# MES20
ros2 topic hz /tbmem_img
ros2 topic hz /detections
ros2 topic echo /detections --once
```

启动成功时，YOLO 节点日志类似：

```text
[ai_infer_node]: loaded yolo backend on NPU core 0
[ai_infer_node]: unified AI node ready: backend=yolo model=yolo11 input=tb_img:/tbmem_img workers=1 strategy=least_loaded queue=2 drop=drop_oldest detections=/detections classifications=/classifications
[ai_infer_node]: AI worker core=0 healthy=true in_flight=0 assigned=0 completed=0 failed=0 dropped=0 latency_ewma=0.00ms
```

# 接口说明

## ROS 话题

| 方向 | 话题 | 消息类型 | 说明 |
|------|------|----------|------|
| 订阅 | `/tbmem_img`（默认） | `tb_img_msgs/TbMsg480P` 等 | `input_msg: tb_img` |
| 订阅 | `/image_raw` | `sensor_msgs/Image` | `input_msg: sensor_image` |
| 发布 | `/detections` | `tb_det_msgs/TbPerceptionTargets` | 原图坐标系检测框 |

`file` 模式不发布 ROS 话题，结果写入 `output_image_file` 指定的 JPG。

## 节点参数

字段字典见 [`config/params_reference.yaml`](config/params_reference.yaml)（不自动加载，供查阅与复制）。按功能分组如下：

| 分组 | 主要参数 | 说明 |
|------|----------|------|
| 输入（在线） | `input_msg`、`input_topic`、`tb_img_profile` | `tb_img` / `sensor_image`；`tb_img_profile` 仅 `tb_img` 时有效 |
| 输入（file） | `image_file`、`output_image_file` | 离线 JPG 路径 |
| 输出 | `publish_detections`、`detections_topic`、`frame_skip` | 检测话题与跳帧 |
| 模型 | `model.backend`、`model.type`、`model.path` | 后端类型（`yolo`）、模型类型（`yolo11`/`yolo26`/`yolov5`/`yolo11_pose`）与 `.nb` 路径 |
| 前处理 | `preprocess.letterbox`、`preprocess.input_width` 等 | letterbox 与输入尺寸 |
| 后处理 | `postprocess.confidence_threshold`、`nms_threshold`、`max_detections` | 置信度、NMS、最大检测数 |
| 调试 | `enable_profile` | 打印 pre / infer / post 耗时 |

场景 yaml 为上述参数的全量实例，一种用法一份文件：

| `params_file` | `input_msg` | 模型 |
|---------------|-------------|------|
| `ai_infer_yolo.yaml` | `tb_img` | 联合测试默认（在线，`model.type` 可改） |
| `tb_img_yolo11_640x480.yaml` | `tb_img` | YOLO11（在线） |
| `tb_img_yolo26_640x480.yaml` | `tb_img` | YOLO26 |
| `tb_img_yolov5_640x480.yaml` | `tb_img` | YOLOv5 |
| `tb_img_yolo11_pose_640x480.yaml` | `tb_img` | YOLO11-Pose（17 关键点） |
| `sensor_image_yolo11_640x480.yaml` | `sensor_image` | YOLO11 |
| `sensor_image_yolov5_640x480.yaml` | `sensor_image` | YOLOv5 |
| `file_yolo11.yaml` | `file` | YOLO11（离线默认） |
| `file_yolo26.yaml` | `file` | YOLO26 |
| `file_yolov5.yaml` | `file` | YOLOv5 |
| `file_yolo11_pose.yaml` | `file` | YOLO11-Pose |

参数合并顺序：`params_file` yaml → launch 覆盖项（`input_topic`、`frame_skip` 等，非空才生效）。详见 `config/README.md`。

## Launch 文件

| Launch | 用途 |
|--------|------|
| `file_cli.launch.py` | 离线 file 模式 |
| `yolo.launch.py` | 在线单节点 |
| `cam_yolo_webviz_demo.launch.py` | 相机 + YOLO + Web 联合测试 |

各 launch 参数：`ros2 launch topsbot_yolo <launch名> -s`

---

## 许可

Apache License 2.0 — 见 [LICENSE](./LICENSE) 与 [NOTICE](./NOTICE)。

欢迎贡献，请参阅 [CONTRIBUTING.md](./CONTRIBUTING.md)。
