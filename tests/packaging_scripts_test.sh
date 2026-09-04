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
expect_rejected env PACKAGE_SUFFIX='invalid_revision' bash "${PACKAGE_SCRIPT}"
expect_rejected env MAINTAINER=$'builder\nInjected: field' bash "${PACKAGE_SCRIPT}"
expect_rejected env BUILD_DIR=/usr/local/cap-lora-build bash "${PACKAGE_SCRIPT}"
expect_rejected env STAGE_DIR=/etc/ssh bash "${PACKAGE_SCRIPT}"
expect_rejected env DIST_DIR=/tmp/cap-lora-dist bash "${PACKAGE_SCRIPT}"
expect_rejected env ROOT_DIR="${ROOT_DIR}/missing" BSP_VERSION='../escape' bash "${DOCKER_SCRIPT}"
expect_rejected env ROOT_DIR="${ROOT_DIR}/missing" BSP_URL='http://example.invalid/sdk.tar.gz' bash "${DOCKER_SCRIPT}"
expect_rejected env ROOT_DIR="${ROOT_DIR}/missing" BSP_VERSION='v9.9.9' BSP_SHA256=not-a-sha bash "${DOCKER_SCRIPT}"

cache_test_root="$(mktemp -d)"
trap 'rm -rf "${cache_test_root}"' EXIT
touch "${cache_test_root}/CMakeLists.txt"
mkdir -p "${cache_test_root}/cache"
archive="${cache_test_root}/cache/sdk_bsp-cache-test.tar.gz"
stamp="${archive}.validated-sha256"
printf 'stale archive\n' >"${archive}"
printf '%064d\n' 1 >"${stamp}"
touch -t 202001010000 "${archive}"
cache_log="${cache_test_root}/cache-test.log"
if env ROOT_DIR="${cache_test_root}" DOCKER_CACHE_DIR="${cache_test_root}/cache" \
    BSP_VERSION=cache-test BSP_URL=https://example.invalid/sdk.tar.gz \
    BSP_SHA256="$(printf '%064d' 0)" bash "${DOCKER_SCRIPT}" >"${cache_log}" 2>&1; then
    echo "Expected stale BSP validation stamp to be rejected" >&2
    exit 1
fi
grep -Fq 'BSP checksum mismatch' "${cache_log}"

grep -Fq 'Exec=@CAP_LORA_EXEC_PATH@' \
    "${ROOT_DIR}/packaging/deb/cap-lora-1262.desktop.in"
grep -Fq 'Icon=cap-lora-1262.png' \
    "${ROOT_DIR}/packaging/deb/cap-lora-1262.desktop.in"
grep -Fq 'KERNEL=="ext_5v_out"' "${ROOT_DIR}/packaging/deb/70-cap-lora-1262.rules"
grep -Fq 'umask 022' "${PACKAGE_SCRIPT}"
grep -Fq 'Version: ${DEBIAN_VERSION}' "${PACKAGE_SCRIPT}"
grep -Fq 'licenses/Montserrat-OFL.txt' "${PACKAGE_SCRIPT}"
if grep -Eq 'chgrp root|chmod g-w' "${PACKAGE_SCRIPT}"; then
    echo "Package removal must not revoke the BSP's shared ext_5v_out permissions" >&2
    exit 1
fi
if [[ -e "${ROOT_DIR}/packaging/deb/m5cardputerzero-cap-lora-1262.sudoers" ]]; then
    echo "Obsolete full-application sudoers rule is still present" >&2
    exit 1
fi

rm -rf "${cache_test_root}"
trap - EXIT

echo "packaging script checks passed"
