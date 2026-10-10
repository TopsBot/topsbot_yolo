#!/usr/bin/env bash
# 阿里云盘分享链接下载公共逻辑（与 model-zoo 保持一致）

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

ALIYUN_DEBUG="${ALIYUN_DEBUG:-false}"

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1" >&2
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1" >&2
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1" >&2
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1" >&2
}

log_debug() {
    if [ "$ALIYUN_DEBUG" = "true" ]; then
        echo -e "[DEBUG] $1" >&2
    fi
}

check_download_deps() {
    local dep
    for dep in wget unzip curl jq; do
        if ! command -v "$dep" >/dev/null 2>&1; then
            log_error "Please install $dep on your system!"
            return 1
        fi
    done
}

parse_share_id() {
    local share_url="$1"

    if [[ "$share_url" =~ /s/([a-zA-Z0-9]+)(\?|$) ]]; then
        echo "${BASH_REMATCH[1]}"
        return 0
    fi

    log_error "Unable to parse share_id from the share link"
    return 1
}

extract_domain() {
    local share_url="$1"

    if [[ "$share_url" =~ domainId=([a-zA-Z0-9]+) ]]; then
        echo "${BASH_REMATCH[1]}"
        return 0
    fi

    local domain
    domain=$(echo "$share_url" | sed -E 's|^https?://||')
    local subdomain
    subdomain=$(echo "$domain" | cut -d'.' -f1)

    if [ -z "$subdomain" ]; then
        log_error "Unable to extract domain from the share link"
        return 1
    fi

    echo "$subdomain"
}

get_share_token() {
    local domain="$1"
    local share_id="$2"

    log_debug "Getting x-share-token..."

    local api_base="https://${domain}.api.aliyunfile.com"
    local url="${api_base}/v2/share_link/get_share_token"

    local data
    data=$(jq -n --arg share_id "$share_id" '{
        "share_id": $share_id,
        "expire_sec": 7200
    }')

    local temp_response
    temp_response=$(mktemp)

    local http_code
    http_code=$(curl -s -o "$temp_response" -w "%{http_code}" -X POST "$url" \
        -H "Content-Type: application/json" \
        -H "Accept: application/json" \
        -H "User-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:136.0) Gecko/20100101 Firefox/136.0" \
        -d "$data")

    local response_body
    response_body=$(cat "$temp_response" 2>/dev/null || echo "")
    rm -f "$temp_response"

    if ! [[ "$http_code" =~ ^[0-9]+$ ]]; then
        log_error "Invalid HTTP status code: $http_code"
        return 1
    fi

    if [ "$http_code" -eq 200 ]; then
        local share_token
        share_token=$(echo "$response_body" | jq -r '.share_token')
        if [ "$share_token" != "null" ] && [ -n "$share_token" ]; then
            echo "$share_token"
            return 0
        fi
    fi

    log_error "Failed to get share token"
    return 1
}

get_file_id_from_share() {
    local domain="$1"
    local share_id="$2"
    local share_token="$3"

    log_debug "Getting file ID from share..."

    local api_base="https://${domain}.api.aliyunfile.com"
    local url="${api_base}/v2/file/list"

    local data
    data=$(jq -n --arg share_id "$share_id" '{
        "share_id": $share_id,
        "parent_file_id": "root",
        "limit": 100
    }')

    local temp_response
    temp_response=$(mktemp)

    local http_code
    http_code=$(curl -s -o "$temp_response" -w "%{http_code}" -X POST "$url" \
        -H "x-share-token: $share_token" \
        -H "Content-Type: application/json" \
        -H "Accept: application/json" \
        -H "User-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:136.0) Gecko/20100101 Firefox/136.0" \
        -d "$data")

    local response_body
    response_body=$(cat "$temp_response" 2>/dev/null || echo "")
    rm -f "$temp_response"

    if ! [[ "$http_code" =~ ^[0-9]+$ ]]; then
        log_error "Invalid HTTP status code: $http_code"
        return 1
    fi

    if [ "$http_code" -eq 200 ]; then
        local file_id
        file_id=$(echo "$response_body" | jq -r '.items[0].file_id')
        if [ "$file_id" != "null" ] && [ -n "$file_id" ]; then
            local file_name
            file_name=$(echo "$response_body" | jq -r '.items[0].name')
            log_debug "Found file: $file_name with ID: $file_id"
            echo "$file_id"
            return 0
        fi
        log_error "No files found in the share link"
        return 1
    fi

    log_error "Failed to get file list"
    return 1
}

get_auto_file_id() {
    local share_url="$1"
    local file_type="$2"

    log_info "Auto getting $file_type file ID..."

    local share_id
    share_id=$(parse_share_id "$share_url") || return 1

    local domain
    domain=$(extract_domain "$share_url") || return 1

    local share_token
    share_token=$(get_share_token "$domain" "$share_id") || return 1

    get_file_id_from_share "$domain" "$share_id" "$share_token"
}

get_download_url() {
    local domain="$1"
    local file_id="$2"
    local share_id="$3"
    local share_token="$4"

    local api_base="https://${domain}.api.aliyunfile.com"
    local url="${api_base}/v2/file/get_download_url"

    local data
    data=$(jq -n --arg share_id "$share_id" --arg file_id "$file_id" '{
        "share_id": $share_id,
        "file_id": $file_id,
        "expire_sec": 7200
    }')

    local temp_response
    temp_response=$(mktemp)

    local http_code
    http_code=$(curl --http1.1 -s -o "$temp_response" -w "%{http_code}" -X POST "$url" \
        -H "x-share-token: $share_token" \
        -H "Content-Type: application/json" \
        -H "Accept: application/json,text/plain,*/*" \
        -H "Origin: https://${domain}.apps.aliyunfile.com" \
        -H "Referer: https://${domain}.apps.aliyunfile.com/" \
        -H "User-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:136.0) Gecko/20100101 Firefox/136.0" \
        -d "$data")

    local response_body
    response_body=$(cat "$temp_response")
    rm -f "$temp_response"

    if ! [[ "$http_code" =~ ^[0-9]+$ ]]; then
        log_error "Invalid HTTP status code: $http_code"
        return 1
    fi

    if [ "$http_code" -eq 200 ]; then
        local download_url
        download_url=$(echo "$response_body" | jq -r '.url')
        if [ "$download_url" != "null" ] && [ -n "$download_url" ]; then
            echo "$download_url"
            return 0
        fi
    fi

    log_error "Failed to get download URL"
    return 1
}

download_aliyun_file() {
    local share_url="$1"
    local file_id="$2"
    local output="$3"

    log_debug "Starting to process Aliyun download: $share_url"

    local share_id
    share_id=$(parse_share_id "$share_url") || return 1

    local domain
    domain=$(extract_domain "$share_url") || return 1

    local share_token
    share_token=$(get_share_token "$domain" "$share_id") || return 1

    local download_url
    download_url=$(get_download_url "$domain" "$file_id" "$share_id" "$share_token") || return 1

    log_debug "Starting to download file to: $output"
    if curl -L -o "$output" "$download_url"; then
        log_success "Download successful: $output"
        return 0
    fi

    log_error "Download failed: $share_url"
    return 1
}
