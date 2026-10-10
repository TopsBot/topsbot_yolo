# Copyright 2026 TOPSBOT contributors.
# SPDX-License-Identifier: Apache-2.0
#
# Offline single-image YOLO (yolo_file_cli).
# Merge order: params_file → image_file / output_image_file overrides.

import os
import sys

from ament_index_python.packages import get_package_prefix, get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, ExecuteProcess, LogInfo, OpaqueFunction
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.substitutions import FindPackageShare

_launch_dir = os.path.dirname(os.path.abspath(__file__))
if _launch_dir not in sys.path:
    sys.path.insert(0, _launch_dir)
from model_utils import ensure_models, resolve_params_file  # noqa: E402


def _load_yaml_params(path: str) -> dict:
    import yaml
    with open(path, 'r', encoding='utf-8') as handle:
        data = yaml.safe_load(handle) or {}
    return data.get('/**', {}).get('ros__parameters', data)


def _nested(params: dict, key: str, default=None):
    node = params
    for part in key.split('.'):
        if not isinstance(node, dict) or part not in node:
            return default
        node = node[part]
    return node


def _launch_setup(context, *args, **kwargs):
    ensure_models(context)

    pkg_share = get_package_share_directory('topsbot_yolo')
    params_path = resolve_params_file(
        pkg_share, LaunchConfiguration('params_file').perform(context))
    params = _load_yaml_params(params_path)

    image_file = LaunchConfiguration('image_file').perform(context)
    output_file = LaunchConfiguration('output_image_file').perform(context) or str(
        params.get('output_image_file', '/tmp/yolo_out.jpg'))

    prefix = get_package_prefix('topsbot_yolo')
    cli = os.path.join(prefix, 'lib', 'topsbot_yolo', 'yolo_file_cli')

    cmd = [
        cli,
        image_file,
        output_file,
        '--model-path', os.path.expanduser(str(params.get('model_path', ''))),
        '--model-type', str(params.get('model_type', 'yolo11')),
        '--conf', str(_nested(params, 'postprocess.confidence_threshold', 0.25)),
        '--nms', str(_nested(params, 'postprocess.nms_threshold', 0.45)),
        '--max-dets', str(int(_nested(params, 'postprocess.max_detections', 300))),
    ]

    return [
        LogInfo(msg=f'[file_cli] params_file: {params_path}'),
        LogInfo(
            msg=(
                f'[file_cli] model_type={params.get("model_type")} '
                f'conf={_nested(params, "postprocess.confidence_threshold", 0.25)} '
                f'in={image_file} out={output_file}'
            )),
        ExecuteProcess(cmd=cmd, output='screen'),
    ]


def generate_launch_description():
    default_image = PathJoinSubstitution([
        FindPackageShare('topsbot_yolo'),
        'imgs',
        'test.jpg',
    ])

    return LaunchDescription([
        DeclareLaunchArgument(
            'params_file',
            default_value='file_yolo11.yaml',
            description='Full file-mode yaml under config/',
        ),
        DeclareLaunchArgument(
            'image_file',
            default_value=default_image,
            description='Input JPEG (default: share/topsbot_yolo/imgs/test.jpg)',
        ),
        DeclareLaunchArgument(
            'output_image_file',
            default_value='',
            description='Output JPG path; empty = use params_file output_image_file',
        ),
        DeclareLaunchArgument(
            'models_dir',
            default_value='~/topsbot/models',
            description='Directory for YOLO .nb model files',
        ),
        DeclareLaunchArgument(
            'auto_download_models',
            default_value='true',
            description='Download missing models before running',
        ),
        OpaqueFunction(function=_launch_setup),
    ])
