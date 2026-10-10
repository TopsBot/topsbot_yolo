#!/usr/bin/env bash

# Expand leading ~ to $HOME (~/path or ~ only).
expand_user_path() {
    local path="$1"
    case "$path" in
        "~/"*)
            echo "${HOME}${path:1}"
            ;;
        "~")
            echo "${HOME}"
            ;;
        *)
            echo "$path"
            ;;
    esac
}
