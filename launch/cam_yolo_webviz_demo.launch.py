# Copyright 2026 TOPSBOT
# SPDX-License-Identifier: Apache-2.0

"""Full live pipeline: usb_cam -> YOLO (via ai_infer) -> WebViz overlay."""

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription, OpaqueFunction
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch.utilities import perform_substitutions
from launch_ros.substitutions import FindPackageShare


def _as_bool(value: str) -> bool:
    return value.lower() in ("true", "1", "yes")


def _launch_setup(context, *args, **kwargs):
    usb_params = LaunchConfiguration("usb_cam_params_file").perform(context)
    ai_infer_params = LaunchConfiguration("ai_infer_params_file").perform(context)
    webviz_params = LaunchConfiguration("webviz_params_file").perform(context)
    http_port = LaunchConfiguration("http_port").perform(context)
    enable_webviz = _as_bool(LaunchConfiguration("enable_webviz").perform(context))
    webviz_enable_tacv = LaunchConfiguration("webviz_enable_tacv_jpeg").perform(context)

    usb_cam_launch = perform_substitutions(context, [
        PathJoinSubstitution([
            FindPackageShare("topsbot_usb_cam"), "launch", "usb_cam.launch.py",
        ]),
    ])
    yolo_launch = perform_substitutions(context, [
        PathJoinSubstitution([
            FindPackageShare("topsbot_yolo"), "launch", "yolo.launch.py",
        ]),
    ])
    webviz_launch = perform_substitutions(context, [
        PathJoinSubstitution([
            FindPackageShare("topsbot_webviz"), "launch", "webviz.launch.py",
        ]),
    ])

    actions = [
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(usb_cam_launch),
            launch_arguments={"params_file": usb_params}.items(),
        ),
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(yolo_launch),
            launch_arguments={"params_file": ai_infer_params}.items(),
        ),
    ]

    if enable_webviz:
        webviz_launch_args = {
            "params_file": webviz_params,
            "http_port": http_port,
        }
        # Always forward enable_tacv_jpeg so webviz doesn't fall back to its
        # yaml default (true). Use non-empty string check like gesture demo.
        if webviz_enable_tacv:
            webviz_launch_args["enable_tacv_jpeg"] = webviz_enable_tacv
        actions.append(
            IncludeLaunchDescription(
                PythonLaunchDescriptionSource(webviz_launch),
                launch_arguments=webviz_launch_args.items(),
            )
        )
    return actions


def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument(
            "usb_cam_params_file",
            default_value="mjpeg2nv12_640x480_zc.yaml",
            description=(
                "topsbot_usb_cam scene YAML. Must produce NV12 zero-copy TbMsg "
                "(phys_addr + nv12 encoding): ai_infer's tb_img path only "
                "accepts nv12/yuv420, and the YOLO backend feeds the NPU "
                "zero-copy via phys_addr LOOKUP."
            ),
        ),
        DeclareLaunchArgument(
            "ai_infer_params_file",
            default_value=PathJoinSubstitution([
                FindPackageShare("topsbot_yolo"), "config", "ai_infer_yolo.yaml",
            ]),
            description="topsbot_ai_infer parameter YAML for YOLO backend",
        ),
        DeclareLaunchArgument(
            "enable_webviz",
            default_value="true",
            description="Start browser visualization",
        ),
        DeclareLaunchArgument(
            "webviz_params_file",
            default_value="tb_img_yolo.yaml",
            description="topsbot_webviz YOLO overlay YAML",
        ),
        DeclareLaunchArgument(
            "http_port",
            default_value="8000",
            description="WebViz static HTTP port",
        ),
        DeclareLaunchArgument(
            "webviz_enable_tacv_jpeg",
            default_value="false",
            description=(
                "Pass-through to topsbot_webviz/launch/webviz.launch.py. "
                "Default 'false' avoids 'Failed to open /dev/carveout-lookup' "
                "spam on boards without ta-cv dmabuf heap."
            ),
        ),
        OpaqueFunction(function=_launch_setup),
    ])
