#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib/aliyun_download.sh
source "${SCRIPT_DIR}/lib/aliyun_download.sh"
# shellcheck source=lib/path_utils.sh
source "${SCRIPT_DIR}/lib/path_utils.sh"

MANIFEST="${SCRIPT_DIR}/../config/models_manifest.yaml"
INSTALL_DIR=""
MODELS_ZIP_URL=""
DEBUG="false"

# Temporary files/dirs to clean up on exit.
TMP_CLEANUP=()
cleanup() {
    local path
    for path in "${TMP_CLEANUP[@]}"; do
        rm -rf "$path"
    done
}
trap cleanup EXIT

usage() {
    cat <<EOF
Usage: $(basename "$0") [OPTIONS]

Download model zips from Aliyun and extract the declared .nb files into install_dir.

Manifest layout:
  model_sources:            # list of archives to download (by URL, deduped)
    - url: <share url>
      files: [<basename>.nb, ...]   # archive members to install (by basename)
  required_models: [...]    # final files that must exist in install_dir

Options:
  --manifest PATH   Manifest yaml (default: package config/models_manifest.yaml)
  --install-dir DIR Override install_dir from manifest
  --url URL         Download a single archive (legacy: extract all .nb/.json)
  --debug           Enable Aliyun API debug logs
  -h, --help        Show this help
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

# Emit model_sources as a JSON array of {"url": ..., "files": [...]|null}.
# Skips empty URLs and duplicates (first occurrence wins).
read_model_sources_json() {
    local manifest="$1"
    python3 - "$manifest" <<'PY'
import json
import sys

try:
    import yaml
except ImportError:
    print("ERROR: python3-yaml is required to read models_manifest.yaml", file=sys.stderr)
    sys.exit(2)

manifest_path = sys.argv[1]
with open(manifest_path, "r", encoding="utf-8") as handle:
    data = yaml.safe_load(handle) or {}

sources = data.get("model_sources") or []
out = []
seen = set()
for source in sources:
    if isinstance(source, str):
        url, files = source, None
    else:
        url = source.get("url", "")
        files = source.get("files")
    if not url:
        continue
    if url in seen:
        continue
    seen.add(url)
    out.append({"url": url, "files": files if isinstance(files, list) else None})
print(json.dumps(out))
PY
}

# Download one archive and copy the wanted members into INSTALL_DIR.
# file_list: newline-separated basenames; empty/"null" means extract every .nb/.json.
download_and_extract_one() {
    local share_url="$1"
    local file_list="$2"

    local file_id
    file_id=$(get_auto_file_id "$share_url" "models") || return 1

    local zip_path extract_dir
    zip_path="$(mktemp /tmp/topsbot_models.XXXXXX.zip)"
    extract_dir="$(mktemp -d /tmp/topsbot_models_extract.XXXXXX)"
    TMP_CLEANUP+=("$zip_path" "$extract_dir")

    download_aliyun_file "$share_url" "$file_id" "$zip_path" || return 1
    unzip -o -q "$zip_path" -d "$extract_dir" || {
        log_error "Failed to unzip archive for $share_url"
        return 1
    }

    mkdir -p "$INSTALL_DIR"
    if [ -z "$file_list" ] || [ "$file_list" = "null" ]; then
        while IFS= read -r found; do
            cp -f "$found" "$INSTALL_DIR/"
            log_success "Ready: ${INSTALL_DIR}/$(basename "$found")"
        done < <(find "$extract_dir" -type f \( -name '*.nb' -o -name '*.json' \))
    else
        local fname found
        while IFS= read -r fname; do
            [ -z "$fname" ] && continue
            found="$(find "$extract_dir" -type f -name "$fname" -print -quit)"
            if [ -n "$found" ]; then
                cp -f "$found" "$INSTALL_DIR/"
                log_success "Ready: ${INSTALL_DIR}/${fname}"
            else
                log_error "Not found in archive: $fname"
            fi
        done <<< "$file_list"
    fi
}

while [ $# -gt 0 ]; do
    case "$1" in
        --manifest)
            MANIFEST="$2"
            shift 2
            ;;
        --install-dir)
            INSTALL_DIR="$2"
            shift 2
            ;;
        --url)
            MODELS_ZIP_URL="$2"
            shift 2
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
            log_error "Unknown option: $1"
            usage
            exit 1
            ;;
    esac
done

if [ "$DEBUG" = "true" ]; then
    ALIYUN_DEBUG="true"
fi

if [ ! -f "$MANIFEST" ]; then
    log_error "Manifest not found: $MANIFEST"
    exit 1
fi

if [ -z "$INSTALL_DIR" ]; then
    INSTALL_DIR="${TOPSBOT_MODEL_DIR:-$(read_manifest_field "$MANIFEST" install_dir)}"
fi
INSTALL_DIR="$(expand_user_path "$INSTALL_DIR")"

check_download_deps

log_info "Install dir: $INSTALL_DIR"

if [ -n "$MODELS_ZIP_URL" ]; then
    # Legacy single-URL mode: download one archive, extract all model files.
    if [[ "$MODELS_ZIP_URL" == *"PLACEHOLDER"* ]]; then
        log_error "url still contains PLACEHOLDER"
        exit 1
    fi
    download_and_extract_one "$MODELS_ZIP_URL" ""
else
    sources_json="$(read_model_sources_json "$MANIFEST")"
    source_count="$(echo "$sources_json" | jq 'length')"
    if [ "$source_count" -eq 0 ]; then
        # Backward compatibility: fall back to models_zip_url.
        MODELS_ZIP_URL="$(read_manifest_field "$MANIFEST" models_zip_url 2>/dev/null || true)"
        if [ -n "$MODELS_ZIP_URL" ]; then
            download_and_extract_one "$MODELS_ZIP_URL" ""
        else
            log_error "No model_sources or models_zip_url in manifest"
            exit 1
        fi
    else
        for index in $(seq 0 $((source_count - 1))); do
            url="$(echo "$sources_json" | jq -r ".[$index].url")"
            files="$(echo "$sources_json" | jq -r ".[$index].files | select(. != null) | join(\"\n\")")"
            download_and_extract_one "$url" "$files"
        done
    fi
fi

mapfile -t REQUIRED_MODELS < <(read_manifest_field "$MANIFEST" required_models)
missing=0
for model in "${REQUIRED_MODELS[@]}"; do
    if [ ! -f "${INSTALL_DIR}/${model}" ]; then
        log_error "Missing after extract: ${INSTALL_DIR}/${model}"
        missing=1
    else
        log_success "Ready: ${INSTALL_DIR}/${model}"
    fi
done

if [ "$missing" -ne 0 ]; then
    log_error "Downloaded archives do not cover required_models in manifest"
    exit 1
fi

log_success "All models installed under $INSTALL_DIR"