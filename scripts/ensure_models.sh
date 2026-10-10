#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DOWNLOAD_SCRIPT="${SCRIPT_DIR}/download_models.sh"
# shellcheck source=lib/path_utils.sh
source "${SCRIPT_DIR}/lib/path_utils.sh"

MANIFEST="${SCRIPT_DIR}/../config/models_manifest.yaml"
INSTALL_DIR=""
AUTO_DOWNLOAD="true"
DEBUG="false"

usage() {
    cat <<EOF
Usage: $(basename "$0") [OPTIONS]

Check required YOLO .nb models under install_dir; download if missing.

Options:
  --manifest PATH       Manifest yaml
  --models-dir DIR      Model install directory
  --auto-download VAL   true|false (default: true)
  --no-download         Same as --auto-download false
  --debug               Pass --debug to download_models.sh
  -h, --help            Show this help
EOF
}

read_manifest_field() {
    local manifest="$1"
    local field="$2"
    python3 - "$manifest" "$field" <<'PY'
import sys

try:
    import yaml
except ImportError:
    print("ERROR: python3-yaml is required to read models_manifest.yaml", file=sys.stderr)
    sys.exit(2)

manifest_path, field = sys.argv[1], sys.argv[2]
with open(manifest_path, "r", encoding="utf-8") as handle:
    data = yaml.safe_load(handle) or {}

value = data.get(field)
if value is None:
    sys.exit(1)
if isinstance(value, list):
    for item in value:
        print(item)
else:
    print(value)
PY
}

while [ $# -gt 0 ]; do
    case "$1" in
        --manifest)
            MANIFEST="$2"
            shift 2
            ;;
        --models-dir|--install-dir)
            INSTALL_DIR="$2"
            shift 2
            ;;
        --auto-download)
            AUTO_DOWNLOAD="$2"
            shift 2
            ;;
        --no-download)
            AUTO_DOWNLOAD="false"
            shift
            ;;
        --debug)
            DEBUG="true"
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "[ERROR] Unknown option: $1" >&2
            usage
            exit 1
            ;;
    esac
done

if [ ! -f "$MANIFEST" ]; then
    echo "[ERROR] Manifest not found: $MANIFEST" >&2
    exit 1
fi

if [ -z "$INSTALL_DIR" ]; then
    INSTALL_DIR="${TOPSBOT_MODEL_DIR:-$(read_manifest_field "$MANIFEST" install_dir)}"
fi
INSTALL_DIR="$(expand_user_path "$INSTALL_DIR")"

mapfile -t REQUIRED_MODELS < <(read_manifest_field "$MANIFEST" required_models)
missing=()
for model in "${REQUIRED_MODELS[@]}"; do
    if [ ! -f "${INSTALL_DIR}/${model}" ]; then
        missing+=("$model")
    fi
done

if [ "${#missing[@]}" -eq 0 ]; then
    echo "[INFO] All required models present under ${INSTALL_DIR}"
    exit 0
fi

echo "[INFO] Missing models under ${INSTALL_DIR}: ${missing[*]}"

case "${AUTO_DOWNLOAD,,}" in
    true|1|yes)
        download_args=(--manifest "$MANIFEST" --install-dir "$INSTALL_DIR")
        if [ "$DEBUG" = "true" ]; then
            download_args+=(--debug)
        fi
        exec "$DOWNLOAD_SCRIPT" "${download_args[@]}"
        ;;
    *)
        echo "[ERROR] Model files missing and auto download disabled." >&2
        echo "[ERROR] Run: $DOWNLOAD_SCRIPT --install-dir $INSTALL_DIR" >&2
        exit 1
        ;;
esac
