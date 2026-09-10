#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 || ! -x "$1" ]]; then
    echo "usage: $0 <cap-lora-executable>" >&2
    exit 2
fi

APP_BIN="$1"
for signal in INT TERM; do
    log_file="$(mktemp)"
    trap 'rm -f "${log_file}"' EXIT
    if ! timeout --preserve-status --signal="${signal}" --kill-after=2.5s 0.5s \
        env SDL_VIDEODRIVER=offscreen LV_SDL_ZOOM=1 "${APP_BIN}" >"${log_file}" 2>&1; then
        echo "Cap-LoRa-1262 did not exit cleanly after SIG${signal}" >&2
        cat "${log_file}" >&2
        exit 1
    fi
    grep -Fq "shutdown complete" "${log_file}"
    if grep -Fq "shutdown timed out" "${log_file}"; then
        echo "Cap-LoRa-1262 hit its forced shutdown path after SIG${signal}" >&2
        exit 1
    fi
    rm -f "${log_file}"
    trap - EXIT
done
