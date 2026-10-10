# Copyright 2026 TOPSBOT
# SPDX-License-Identifier: Apache-2.0

"""Standalone YOLO launch (via topsbot_ai_infer)."""

import os
import sys

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription, OpaqueFunction
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration

# Add launch directory to path for model_utils import
_launch_dir = os.path.dirname(os.path.abspath(__file__))
if _launch_dir not in sys.path:
    sys.path.insert(0, _launch_dir)
from model_utils import ensure_yolo_models, resolve_params_file  # noqa: E402


def _resolve_params_file_arg(context) -> str:
    """params_file is primary; config_file kept for backward compatibility."""
    legacy = LaunchConfiguration('config_file').perform(context)
    if legacy:
        return legacy
    return LaunchConfiguration('params_file').perform(context)


def _launch_setup(context):
    """Ensure models are downloaded, then hand params + overrides to ai_infer.

    Merge order matches README: params_file yaml -> launch overrides
    (input_topic / input_msg / tb_img_profile / frame_skip, non-empty only).
    """
    ensure_yolo_models(context)

    pkg_share = get_package_share_directory('topsbot_yolo')
    params_path = resolve_params_file(pkg_share, _resolve_params_file_arg(context))

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

    frame_skip = LaunchConfiguration('frame_skip').perform(context)
    if frame_skip:
        try:
            overrides['frame_skip'] = int(frame_skip)
        except ValueError:
            pass

    ai_infer_launch = os.path.join(
        get_package_share_directory('topsbot_ai_infer'), 'launch', 'ai_infer.launch.py')

    launch_args = {'params_file': params_path}
    launch_args.update(overrides)

    return [
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(ai_infer_launch),
            launch_arguments=launch_args.items(),
        ),
    ]


def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument(
            'params_file',
            default_value='ai_infer_yolo.yaml',
            description='Scene yaml under topsbot_yolo/config (or absolute path)',
        ),
        DeclareLaunchArgument(
            'config_file',
            default_value='',
            description='Deprecated: alias for params_file',
        ),
        DeclareLaunchArgument(
            'auto_download_models',
            default_value='true',
            description='Auto download models if missing',
        ),
        DeclareLaunchArgument(
            'input_topic',
            default_value='',
            description='Override subscription topic',
        ),
        DeclareLaunchArgument(
            'input_msg',
            default_value='',
            description='Override input type: tb_img | sensor_image | file',
        ),
        DeclareLaunchArgument(
            'tb_img_profile',
            default_value='',
            description='Override TbMsg tier: 480p | 540p | 1080p | auto',
        ),
        DeclareLaunchArgument(
            'frame_skip',
            default_value='',
            description='Override frame skip (0 = no skip)',
        ),
        OpaqueFunction(function=_launch_setup),
    ])