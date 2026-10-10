# topsbot_yolo 快速入门

完整说明见 `README_cn.md`（English: `README.md`）。

## 1. 离线 file 模式

```bash
# MES20 — 默认
ros2 launch topsbot_yolo file_cli.launch.py

# MES20 — 改模型 / 输入 / 输出
ros2 launch topsbot_yolo file_cli.launch.py \
  params_file:=file_yolo26.yaml \
  image_file:=/path/to/in.jpg \
  output_image_file:=/path/to/out.jpg
```

## 2. 单节点模式（在线）

```bash
# MES20 — 默认（/tbmem_img + YOLO11）
ros2 launch topsbot_yolo yolo.launch.py

# MES20 — YOLO26 / 换话题 / 跳帧
ros2 launch topsbot_yolo yolo.launch.py params_file:=tb_img_yolo26_640x480.yaml
ros2 launch topsbot_yolo yolo.launch.py input_topic:=/cam0/tbmem_img
ros2 launch topsbot_yolo yolo.launch.py frame_skip:=2

# MES20 — sensor_msgs 路径（须与 usb_cam 成对切换）
ros2 launch topsbot_usb_cam usb_cam.launch.py params_file:=mjpeg2nv12_640x480_sensor.yaml setup_shm:=false
ros2 launch topsbot_yolo yolo.launch.py params_file:=sensor_image_yolo11_640x480.yaml
```

## 3. 联合测试（相机 + YOLO + Web）

```bash
# MES20
source /opt/tbros/humble/setup.bash
#source <install>/setup.bash
ros2 launch topsbot_yolo cam_yolo_webviz_demo.launch.py
```

浏览器访问 `http://<板卡IP>:8000/` 查看 NV12 画面与检测框叠加。无 Web：`enable_webviz:=false`。

**确认链路正常**

```bash
# MES20
ros2 topic hz /tbmem_img
ros2 topic hz /detections
ros2 topic echo /detections --once
```

启动成功时，YOLO 节点日志类似：

```text
yolo ready: model_type=yolo11 topic=/tbmem_img detections=/detections
frame #1 pre=... infer=... post=... total=... ms dets=...
```

## 4. 查看 launch 参数

```bash
ros2 launch topsbot_yolo file_cli.launch.py -s
ros2 launch topsbot_yolo yolo.launch.py -s
ros2 launch topsbot_yolo cam_yolo_webviz_demo.launch.py -s
```

## 5. 配置与参数

- 场景 yaml：`config/README.md`
- 参数字典：`config/params_reference.yaml`
