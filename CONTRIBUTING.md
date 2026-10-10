# Contributing

Thank you for contributing to topsbot_yolo.

## Getting Started

1. Fork the repository and create a feature branch from `main`.
2. Install ROS 2 Humble and TOPSBOT board dependencies (`ta_runtime`, `ta-cv`, dmabuf).
3. Build dependencies first:

```bash
colcon build --packages-select tb_img_msgs tb_det_msgs topsbot_usb_cam topsbot_yolo
source install/setup.bash
```

4. For board validation, test on MES20 (or document host-only limitations).

## Model Files

- Do **not** commit `.nb` model binaries or `models.zip`.
- Update `config/models_manifest.yaml` only when the official download URL changes.
- Test download flow with `./scripts/ensure_models.sh` when touching scripts.

## Code Style

- Use English for code comments, log messages, and documentation.
- Match existing naming and module layout under `topsbot_yolo/` and `src/`.
- Prefer focused changes; avoid unrelated refactors in the same MR.
- Default build type is Release (`-O3`) for performance-sensitive paths.

## Pull Requests

- Describe the problem, approach, and test method (board / file CLI / launch demo).
- Update `README.md` when user-facing behavior or launch parameters change.
- Add an entry under `CHANGELOG.md` (Unreleased section).
- Do not commit models, captured images, or local build artifacts.

## Reporting Issues

Please include:

- Board model (e.g. MES20) and ROS 2 distro
- Launch command and config yaml
- Model path and whether auto-download was enabled
- Relevant log excerpts (model load, dmabuf, inference errors)

## License

By contributing, you agree that your contributions will be licensed under the Apache License 2.0.
