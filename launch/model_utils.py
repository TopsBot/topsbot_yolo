# Copyright 2026 TOPSBOT contributors.
# SPDX-License-Identifier: Apache-2.0

"""Shared helpers for YOLO launch files: params resolution + model ensure."""

import os
import subprocess
import sys

from ament_index_python.packages import get_package_share_directory
from launch.substitutions import LaunchConfiguration
from launch.utilities import perform_substitutions
from launch_ros.substitutions import FindPackageShare


def resolve_params_file(pkg_share: str, params_file: str) -> str:
    if not params_file:
        raise RuntimeError('yolo.launch: params_file is empty')
    if os.path.isabs(params_file):
        path = params_file
    else:
        path = os.path.join(pkg_share, 'config', params_file)
    if not os.path.isfile(path):
        raise RuntimeError(f'yolo.launch: params_file not found: {path}')
    return path


def ensure_models(context) -> None:
    """Block until required .nb models exist (download when enabled)."""
    auto = LaunchConfiguration('auto_download_models').perform(context)
    models_dir = os.path.expanduser(LaunchConfiguration('models_dir').perform(context))
    share = perform_substitutions(context, [FindPackageShare('topsbot_yolo')])
    script = os.path.join(share, 'scripts', 'ensure_models.sh')
    manifest = os.path.join(share, 'config', 'models_manifest.yaml')

    cmd = [
        script,
        '--manifest', manifest,
        '--models-dir', models_dir,
    ]
    if auto.lower() not in ('true', '1', 'yes'):
        cmd.append('--no-download')

    proc = subprocess.run(cmd, check=False)
    if proc.returncode != 0:
        raise RuntimeError(
            'YOLO model files are missing or download failed. '
            f'Expected under {models_dir}. '
            'Run download_models.sh manually or launch with auto_download_models:=true.')


def ensure_yolo_models(context):
    """Ensure YOLO models are downloaded before launching.

    Non-fatal variant used by yolo.launch.py: warns instead of raising so the
    node still starts with an empty model dir (it will re-check on its own).
    """
    auto_download = context.launch_configurations.get('auto_download_models', 'true')
    if not isinstance(auto_download, str):
        auto_download = perform_substitutions(context, [auto_download])
    if auto_download.lower() not in ('true', '1', 'yes'):
        return

    pkg_share = get_package_share_directory('topsbot_yolo')
    ensure_script = os.path.join(pkg_share, 'scripts', 'ensure_models.sh')

    if not os.path.isfile(ensure_script):
        # Fallback: try source directory
        pkg_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        ensure_script = os.path.join(pkg_dir, 'scripts', 'ensure_models.sh')

    if not os.path.isfile(ensure_script):
        print(f"[WARN] ensure_models.sh not found, skipping model check", file=sys.stderr)
        return

    manifest = os.path.join(pkg_share, 'config', 'models_manifest.yaml')
    if not os.path.isfile(manifest):
        manifest = os.path.join(os.path.dirname(ensure_script), '..', 'config', 'models_manifest.yaml')

    try:
        result = subprocess.run(
            ['bash', ensure_script, '--manifest', manifest],
            capture_output=True,
            text=True,
            timeout=300
        )
        if result.returncode != 0:
            print(f"[WARN] Model ensure failed: {result.stderr}", file=sys.stderr)
        elif result.stdout:
            print(f"[INFO] {result.stdout.strip()}", file=sys.stderr)
    except subprocess.TimeoutExpired:
        print("[WARN] Model download timed out", file=sys.stderr)
    except Exception as e:
        print(f"[WARN] Model check failed: {e}", file=sys.stderr)