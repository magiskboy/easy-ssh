#!/usr/bin/env bash
# Build lxqt-build-tools + qtermwidget into $PREFIX (Unix / macOS).
set -euo pipefail

PREFIX="${PREFIX:?PREFIX must be set}"
CMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH:-$PREFIX}"
BUILD_ROOT="${BUILD_ROOT:-$GITHUB_WORKSPACE/.deps/src}"
JOBS="${JOBS:-$(nproc 2>/dev/null || sysctl -n hw.ncpu)}"

LXQT_BT_REF="${LXQT_BT_REF:-2.4.0}"
QTERMWIDGET_REF="${QTERMWIDGET_REF:-2.4.0}"

mkdir -p "$BUILD_ROOT" "$PREFIX"
export CMAKE_PREFIX_PATH="$PREFIX:${CMAKE_PREFIX_PATH}"
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig:$PREFIX/lib64/pkgconfig:${PKG_CONFIG_PATH:-}"
export PATH="$PREFIX/bin:$PATH"

cmake_build_install() {
  local src="$1"
  local name
  name="$(basename "$src")"
  cmake -S "$src" -B "$src/build" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DCMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH" \
    "${@:2}"
  cmake --build "$src/build" --parallel "$JOBS"
  cmake --install "$src/build"
  echo "Installed $name into $PREFIX"
}

if [[ ! -d "$BUILD_ROOT/lxqt-build-tools" ]]; then
  git clone --depth 1 --branch "$LXQT_BT_REF" \
    https://github.com/lxqt/lxqt-build-tools.git \
    "$BUILD_ROOT/lxqt-build-tools"
fi
cmake_build_install "$BUILD_ROOT/lxqt-build-tools"

if [[ ! -d "$BUILD_ROOT/qtermwidget" ]]; then
  git clone --depth 1 --branch "$QTERMWIDGET_REF" \
    https://github.com/lxqt/qtermwidget.git \
    "$BUILD_ROOT/qtermwidget"
fi
cmake_build_install "$BUILD_ROOT/qtermwidget" \
  -DBUILD_TRANSLATIONS=OFF \
  -DUSE_UTF8PROC=OFF
