#!/usr/bin/env bash
set -euo pipefail

HOME="${HOME:-/tmp/cap-lora-home}"
ROOT_DIR="${ROOT_DIR:-/workspace}"
BSP_VERSION="${BSP_VERSION:-v0.0.4}"
BSP_URL="${BSP_URL:-https://github.com/CardputerZero/M5CardputerZero-UserDemo/releases/download/${BSP_VERSION}/sdk_bsp.tar.gz}"
BSP_SHA256="${BSP_SHA256:-}"
ALLOW_UNVERIFIED_BSP="${CAP_LORA_ALLOW_UNVERIFIED_BSP:-0}"
if [[ -z "${BSP_SHA256}" && "${BSP_VERSION}" == "v0.0.4" ]]; then
    BSP_SHA256="e51b6eb803ed08f450e459efbfe62dd0341440846f3be9d01da861fe6cfdebb0"
fi
if [[ ! "${BSP_VERSION}" =~ ^[A-Za-z0-9._-]+$ ]]; then
    echo "Invalid BSP_VERSION: ${BSP_VERSION}" >&2
    exit 1
fi
if [[ "${BSP_URL}" != https://* ]]; then
    echo "BSP_URL must use HTTPS." >&2
    exit 1
fi
if [[ -n "${BSP_SHA256}" && ! "${BSP_SHA256}" =~ ^[[:xdigit:]]{64}$ ]]; then
    echo "BSP_SHA256 must be exactly 64 hexadecimal characters." >&2
    exit 1
fi
if [[ -z "${BSP_SHA256}" && "${ALLOW_UNVERIFIED_BSP}" != "1" ]]; then
    echo "BSP_SHA256 is required; set CAP_LORA_ALLOW_UNVERIFIED_BSP=1 only for a trusted local mirror." >&2
    exit 1
fi
BSP_CACHE_KEY="${BSP_VERSION//[^a-zA-Z0-9._-]/_}"
CACHE_DIR="${DOCKER_CACHE_DIR:-${ROOT_DIR}/build/docker-cache}"
ARCHIVE="${CACHE_DIR}/sdk_bsp-${BSP_CACHE_KEY}.tar.gz"
ARCHIVE_VALIDATION_STAMP="${ARCHIVE}.validated-sha256"
SYSROOT="${CACHE_DIR}/sdk_bsp-${BSP_CACHE_KEY}"
SYSROOT_VALIDATION_STAMP="${SYSROOT}/.cap-lora-bsp-sha256"
BUILD_DIR="${ROOT_DIR}/build/package-docker-cross-${BSP_CACHE_KEY}"
STAGE_DIR="${ROOT_DIR}/build/deb-root-docker-cross-${BSP_CACHE_KEY}"

die() {
    echo "$*" >&2
    exit 1
}

safe_remove_tree() {
    local path="$1"
    local boundary="$2"
    local label="$3"
    local resolved boundary_resolved

    [[ -n "${path}" ]] || die "Refusing to remove an empty ${label} path."
    resolved="$(realpath -m -- "${path}")"
    boundary_resolved="$(realpath -m -- "${boundary}")"
    [[ "${resolved}" != "/" && "${resolved}" != "${boundary_resolved}" ]] || \
        die "Refusing to remove unsafe ${label} path: ${path}"
    case "${boundary_resolved}/" in
        "${resolved}"/*) die "Refusing to remove an ancestor of ${boundary}: ${path}" ;;
    esac
    case "${resolved}" in
        "${boundary_resolved}"/*) ;;
        *) die "${label} path must stay below ${boundary}: ${path}" ;;
    esac
    rm -rf -- "${path}"
}

if [[ ! -d "${ROOT_DIR}" || ! -f "${ROOT_DIR}/CMakeLists.txt" ]]; then
    echo "Cap-LoRa-1262 source is not mounted at ${ROOT_DIR}." >&2
    exit 1
fi

mkdir -p "${HOME}" "${CACHE_DIR}"
if ! command -v realpath >/dev/null 2>&1; then
    echo "Required command not found: realpath" >&2
    exit 1
fi
if [[ ! -f "${ARCHIVE}" ]]; then
    echo "Downloading CardputerZero BSP ${BSP_VERSION} (cached after the first build)..."
    if ! curl --fail --location --proto '=https' --tlsv1.2 --retry 3 --continue-at - \
        --output "${ARCHIVE}.part" "${BSP_URL}"; then
        echo "Resuming the BSP download failed; retrying it from the beginning..." >&2
        rm -f -- "${ARCHIVE}.part"
        curl --fail --location --proto '=https' --tlsv1.2 --retry 3 \
            --output "${ARCHIVE}.part" "${BSP_URL}"
    fi
    rm -f "${ARCHIVE_VALIDATION_STAMP}"
    mv "${ARCHIVE}.part" "${ARCHIVE}"
fi

archive_sha256=""
validated_sha256=""
if [[ -f "${ARCHIVE_VALIDATION_STAMP}" ]]; then
    validated_sha256="$(<"${ARCHIVE_VALIDATION_STAMP}")"
fi
if [[ -n "${validated_sha256}" && "${ARCHIVE}" -ot "${ARCHIVE_VALIDATION_STAMP}" ]]; then
    archive_sha256="${validated_sha256}"
else
    archive_sha256="$(sha256sum "${ARCHIVE}")"
    archive_sha256="${archive_sha256%% *}"
    if [[ -n "${BSP_SHA256}" && "${archive_sha256}" != "${BSP_SHA256}" ]]; then
        echo "BSP checksum mismatch: expected ${BSP_SHA256}, got ${archive_sha256}." >&2
        echo "Remove ${ARCHIVE} and retry." >&2
        exit 1
    fi
    printf "%s\n" "${archive_sha256}" >"${ARCHIVE_VALIDATION_STAMP}"
fi

sysroot_sha256=""
if [[ -f "${SYSROOT_VALIDATION_STAMP}" ]]; then
    sysroot_sha256="$(<"${SYSROOT_VALIDATION_STAMP}")"
fi
if [[ ! -d "${SYSROOT}/usr/include" || ! -d "${SYSROOT}/usr/lib" || \
      "${sysroot_sha256}" != "${archive_sha256}" ]]; then
    echo "Preparing CardputerZero BSP sysroot..."
    EXTRACT_DIR="${SYSROOT}.extracting"
    safe_remove_tree "${EXTRACT_DIR}" "${CACHE_DIR}" "BSP extraction"
    safe_remove_tree "${SYSROOT}" "${CACHE_DIR}" "BSP sysroot"
    mkdir -p "${EXTRACT_DIR}"
    if ! tar -tzf "${ARCHIVE}" | awk '$0 ~ /^\// || $0 ~ /(^|\/)\.\.(\/|$)/ { bad=1 } END { exit bad }' || \
       tar -tvzf "${ARCHIVE}" | awk 'substr($0, 1, 1) == "l" || substr($0, 1, 1) == "h" { bad=1 } END { exit bad }'; then
        echo "BSP archive contains an unsafe path or link entry." >&2
        exit 1
    fi
    tar -xzf "${ARCHIVE}" --no-same-owner --no-same-permissions -C "${EXTRACT_DIR}"

    if [[ ! -d "${EXTRACT_DIR}/usr/include" || ! -d "${EXTRACT_DIR}/usr/lib" ]]; then
        echo "Unexpected BSP archive layout: usr/include and usr/lib were not found." >&2
        exit 1
    fi
    printf "%s\n" "${archive_sha256}" >"${EXTRACT_DIR}/.cap-lora-bsp-sha256"
    mv "${EXTRACT_DIR}" "${SYSROOT}"
fi

reset_build="${CLEAN:-0}"
if [[ -f "${BUILD_DIR}/CMakeCache.txt" ]]; then
    cached_source="$(sed -n 's/^CMAKE_HOME_DIRECTORY:[^=]*=//p' "${BUILD_DIR}/CMakeCache.txt")"
    cached_sysroot="$(sed -n 's/^CMAKE_SYSROOT:[^=]*=//p' "${BUILD_DIR}/CMakeCache.txt")"
    if [[ "${cached_source}" != "${ROOT_DIR}" || "${cached_sysroot}" != "${SYSROOT}" ]]; then
        echo "The cached CMake paths belong to another workspace or sysroot; reconfiguring..."
        reset_build=1
    fi
fi
if [[ "${reset_build}" == "1" ]]; then
    echo "Removing cached package build directories..."
    safe_remove_tree "${BUILD_DIR}" "${ROOT_DIR}/build" "package build"
    safe_remove_tree "${STAGE_DIR}" "${ROOT_DIR}/build" "package staging"
fi

cd "${ROOT_DIR}"
python3 -c 'import fetch_repos; fetch_repos.ensure_dependencies()'

CAP_LORA_FORCE_CROSS=1 \
CAP_LORA_SYSROOT="${SYSROOT}" \
BUILD_DIR="${BUILD_DIR}" \
STAGE_DIR="${STAGE_DIR}" \
DIST_DIR="${ROOT_DIR}/dist" \
PARALLEL="${PARALLEL:-4}" \
CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}" \
PACKAGE_SUFFIX="${PACKAGE_SUFFIX:-m5stack1}" \
    "${ROOT_DIR}/packaging/deb/package_deb.sh"
