#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PACKAGE_SCRIPT="${ROOT_DIR}/packaging/deb/package_deb.sh"
DOCKER_SCRIPT="${ROOT_DIR}/packaging/docker/build_package.sh"

for script in "${ROOT_DIR}/bootstrap.sh" "${PACKAGE_SCRIPT}" "${DOCKER_SCRIPT}" \
    "${ROOT_DIR}/packaging/docker/package_deb.sh"; do
    bash -n "${script}"
done

expect_rejected() {
    if "$@" >/dev/null 2>&1; then
        echo "Expected command to be rejected: $*" >&2
        exit 1
    fi
}

expect_rejected env PACKAGE_NAME='cap/lora' bash "${PACKAGE_SCRIPT}"
expect_rejected env PACKAGE_SUFFIX='../escape' bash "${PACKAGE_SCRIPT}"
expect_rejected env MAINTAINER=$'builder\nInjected: field' bash "${PACKAGE_SCRIPT}"
expect_rejected env ROOT_DIR="${ROOT_DIR}/missing" BSP_VERSION='../escape' bash "${DOCKER_SCRIPT}"
expect_rejected env ROOT_DIR="${ROOT_DIR}/missing" BSP_URL='http://example.invalid/sdk.tar.gz' bash "${DOCKER_SCRIPT}"
expect_rejected env ROOT_DIR="${ROOT_DIR}/missing" BSP_VERSION='v9.9.9' BSP_SHA256=not-a-sha bash "${DOCKER_SCRIPT}"

grep -Fq 'Exec=/usr/bin/sudo -n -- @CAP_LORA_EXEC_PATH@' \
    "${ROOT_DIR}/packaging/deb/cap-lora-1262.desktop.in"
grep -Fq '%gpio ALL=(root) NOPASSWD: @CAP_LORA_EXEC_PATH@ ""' \
    "${ROOT_DIR}/packaging/deb/m5cardputerzero-cap-lora-1262.sudoers"

echo "packaging script checks passed"
