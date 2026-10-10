# Copyright 2026 TOPSBOT contributors.
# SPDX-License-Identifier: Apache-2.0
#
# yolo_det.launch.py — YOLO detection via topsbot_ai_infer (ai_infer_node).
#
# Previously launched the standalone yolo_node executable; now delegates to
# yolo.launch.py which routes through ai_infer_node + YoloBackend plugin.
# All public arguments are preserved for backward compatibility.

import os
import sys

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription, OpaqueFunction
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.substitutions import FindPackageShare

_launch_dir = os.path.dirname(os.path.abspath(__file__))
if _launch_dir not in sys.path:
    sys.path.insert(0, _launch_dir)
from model_utils import ensure_models, resolve_params_file  # noqa: E402

_LEGACY_CONFIG_MAP = {
    'yolo11_demo.yaml': 'tb_img_yolo11_640x480.yaml',
    'yolo26_demo.yaml': 'tb_img_yolo26_640x480.yaml',
    'yolo_file_demo.yaml': 'file_yolo11.yaml',
}


def _resolve_params_file_arg(context) -> str:
    scene = LaunchConfiguration('scene_params_file').perform(context)
    if scene:
        return scene
    legacy = LaunchConfiguration('config').perform(context)
    if legacy:
        return _LEGACY_CONFIG_MAP.get(legacy, legacy)
    return LaunchConfiguration('params_file').perform(context)


def _launch_setup(context, *args, **kwargs):
    ensure_models(context)

    pkg_share = get_package_share_directory('topsbot_yolo')
    params_path = resolve_params_file(pkg_share, _resolve_params_file_arg(context))

    # Build override dict from non-empty launch arguments.
    # ai_infer_node uses the same parameter names as the old yolo_node for
    # input_msg / input_topic / frame_skip, so overrides pass through unchanged.
    overrides = {}

    input_topic = LaunchConfiguration('input_topic').perform(context)
    if input_topic:
        overrides['input_topic'] = input_topic

    input_msg = LaunchConfiguration('input_msg').perform(context)
    if input_msg:
        overrides['input_msg'] = input_msg

    tb_img_profile = LaunchConfiguration('tb_img_profile').perform(context)
    if tb_img_profile:
        overrides['tb_img_profile'] = tb_img_profile

    frame_skip_str = LaunchConfiguration('frame_skip').perform(context)
    if frame_skip_str:
        try:
            overrides['frame_skip'] = int(frame_skip_str)
        except ValueError:
            pass

    # Assemble launch arguments for yolo.launch.py. Forward the resolved
    # params path plus the non-empty overrides (yolo.launch.py applies them
    # on top of the yaml like the standalone path does).
    yolo_launch_args = {'params_file': params_path}
    yolo_launch_args.update(overrides)

    yolo_launch = PathJoinSubstitution([
        FindPackageShare('topsbot_yolo'), 'launch', 'yolo.launch.py',
    ])

    actions = [
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(yolo_launch),
            launch_arguments=yolo_launch_args.items(),
        ),
    ]
    return actions


def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument(
            'params_file',
            default_value='tb_img_yolo11_640x480.yaml',
            description='Scene yaml under config/ (or absolute path)',
        ),
        DeclareLaunchArgument(
            'scene_params_file',
            default_value='',
            description='Explicit scene yaml (avoids params_file name collision when included)',
        ),
        DeclareLaunchArgument(
            'config',
            default_value='',
            description='Deprecated alias for params_file',
        ),
        DeclareLaunchArgument(
            'models_dir',
            default_value='~/topsbot/models',
            description='Directory for YOLO .nb model files',
        ),
        DeclareLaunchArgument(
            'auto_download_models',
            default_value='true',
            description='Download missing models before start',
        ),
        DeclareLaunchArgument(
            'input_topic',
            default_value='',
            description='Override input_topic',
        ),
        DeclareLaunchArgument(
            'input_msg',
            default_value='',
            description='Override input_msg: tb_img | sensor_image | file',
        ),
        DeclareLaunchArgument(
            'tb_img_profile',
            default_value='',
            description='Override TbMsg tier: 480p | 540p | 1080p | auto',
        ),
        DeclareLaunchArgument(
            'frame_skip',
            default_value='',
            description='Override frame_skip (0 = no skip)',
        ),
        OpaqueFunction(function=_launch_setup),
    ])
