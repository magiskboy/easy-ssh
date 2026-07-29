#!/usr/bin/env bash
# Build libssh (>= 0.11, ProxyJump) into $PREFIX when the system copy is too old.
set -euo pipefail

PREFIX="${PREFIX:?PREFIX must be set}"
BUILD_ROOT="${BUILD_ROOT:-${GITHUB_WORKSPACE:?}/.deps/src}"
JOBS="${JOBS:-$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)}"
# ProxyJump (SSH_OPTIONS_PROXYJUMP / ssh_jump_callbacks) landed in libssh 0.11.0.
LIBSSH_REF="${LIBSSH_REF:-libssh-0.11.5}"
LIBSSH_MIN_VERSION="${LIBSSH_MIN_VERSION:-0.11.0}"

version_ge() {
    # Returns 0 if $1 >= $2 (dotted numeric versions).
    printf '%s\n%s\n' "$2" "$1" | sort -V | head -n1 | grep -qx "$2"
}

have_new_enough_libssh() {
    local ver=""
    if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists libssh; then
        ver="$(pkg-config --modversion libssh 2>/dev/null || true)"
    fi
    if [[ -z "${ver}" ]]; then
        return 1
    fi
    echo "Found system libssh ${ver}"
    version_ge "${ver}" "${LIBSSH_MIN_VERSION}"
}

if have_new_enough_libssh; then
    echo "System libssh satisfies >= ${LIBSSH_MIN_VERSION}; skipping build"
    exit 0
fi

echo "Building libssh ${LIBSSH_REF} into ${PREFIX} (need >= ${LIBSSH_MIN_VERSION} for ProxyJump)"

mkdir -p "$BUILD_ROOT" "$PREFIX"
export PKG_CONFIG_PATH="${PREFIX}/lib/pkgconfig:${PREFIX}/lib64/pkgconfig:${PKG_CONFIG_PATH:-}"

SRC="${BUILD_ROOT}/libssh"
if [[ ! -d "${SRC}/.git" ]]; then
    rm -rf "${SRC}"
    git clone --depth 1 --branch "${LIBSSH_REF}" \
        https://gitlab.com/libssh/libssh-mirror.git \
        "${SRC}"
fi

cmake -S "${SRC}" -B "${SRC}/build" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="${PREFIX}" \
    -DBUILD_SHARED_LIBS=ON \
    -DWITH_SERVER=ON \
    -DWITH_EXAMPLES=OFF \
    -DUNIT_TESTING=OFF \
    -DWITH_GSSAPI=OFF

cmake --build "${SRC}/build" --parallel "${JOBS}"
cmake --install "${SRC}/build"

echo "Installed libssh ${LIBSSH_REF} into ${PREFIX}"
pkg-config --modversion libssh || true
