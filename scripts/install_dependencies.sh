#!/usr/bin/env bash

set -Eeuo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

OPENCV_VERSION="${OPENCV_VERSION:-4.8.0}"
OPENCV_ROOT="${OPENCV_ROOT:-}"
QT_ROOT="${QT_ROOT:-}"
HALCON_ROOT="${HALCONROOT:-${HALCON_ROOT:-}}"
HALCON_LICENSE="${HALCON_LICENSE_FILE:-}"
JOBS="${JOBS:-$(nproc)}"
INSTALL_SYSTEM_PACKAGES=1
BUILD_OPENCV=1
CHECK_ONLY=0

usage() {
    cat <<'EOF'
Usage: scripts/install_dependencies.sh [options]

Install/check the dependencies required by qt_ui_test.pro on Ubuntu 22.04+.

Options:
  --check-only          Do not install anything; only validate dependencies
  --skip-system         Do not install packages with apt
  --skip-opencv-build   Do not build OpenCV when the target prefix is missing
  --opencv-root PATH    OpenCV 4.8 installation prefix
  --qt-root PATH        Qt installation root (must contain bin/qmake)
  --halcon-root PATH    Existing licensed HALCON installation root
  --jobs N              Parallel build jobs (default: number of CPU cores)
  -h, --help            Show this help

HALCON is commercial software and is intentionally not downloaded by this
script. This branch requires HALCON 20.11. Install it using the MVTec installer
and pass --halcon-root, or export HALCONROOT before running this script.
EOF
}

die() {
    printf 'ERROR: %s\n' "$*" >&2
    exit 1
}

info() {
    printf '\n==> %s\n' "$*"
}

while (($# > 0)); do
    case "$1" in
        --check-only)
            CHECK_ONLY=1
            INSTALL_SYSTEM_PACKAGES=0
            BUILD_OPENCV=0
            shift
            ;;
        --skip-system)
            INSTALL_SYSTEM_PACKAGES=0
            shift
            ;;
        --skip-opencv-build)
            BUILD_OPENCV=0
            shift
            ;;
        --opencv-root)
            (($# >= 2)) || die "--opencv-root requires a path"
            OPENCV_ROOT="$2"
            shift 2
            ;;
        --qt-root)
            (($# >= 2)) || die "--qt-root requires a path"
            QT_ROOT="$2"
            shift 2
            ;;
        --halcon-root)
            (($# >= 2)) || die "--halcon-root requires a path"
            HALCON_ROOT="$2"
            shift 2
            ;;
        --jobs)
            (($# >= 2)) || die "--jobs requires a positive integer"
            JOBS="$2"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            die "unknown option: $1"
            ;;
    esac
done

[[ "$JOBS" =~ ^[1-9][0-9]*$ ]] || die "--jobs must be a positive integer"

REQUESTED_QT_ROOT="$QT_ROOT"
INITIAL_HALCON_ROOT="$HALCON_ROOT"

if [[ -r /etc/os-release ]]; then
    # shellcheck disable=SC1091
    source /etc/os-release
    [[ "${ID:-}" == "ubuntu" || "${ID_LIKE:-}" == *debian* ]] || \
        die "automatic package installation currently supports Ubuntu/Debian only"
else
    die "cannot determine the operating system"
fi

qt_root_is_compatible() {
    local root="$1"
    local qmake="$root/bin/qmake"
    local version headers header
    [[ -x "$qmake" ]] || return 1
    version="$("$qmake" -query QT_VERSION 2>/dev/null || true)"
    [[ "$version" == 5.* ]] || return 1
    headers="$("$qmake" -query QT_INSTALL_HEADERS 2>/dev/null || true)"
    for header in \
        QtWidgets/QWidget \
        QtConcurrent/QtConcurrent \
        QtMultimedia/QMediaPlayer \
        QtMultimediaWidgets/QVideoWidget; do
        [[ -f "$headers/$header" ]] || return 1
    done
}

discover_qt_root() {
    local candidate qmake_path
    for candidate in \
        "$HOME/Qt/5.15.2/gcc_64" \
        /opt/Qt/5.15.2/gcc_64 \
        /usr/lib/qt5; do
        if qt_root_is_compatible "$candidate"; then
            QT_ROOT="$candidate"
            return 0
        fi
    done
    if command -v qmake >/dev/null 2>&1; then
        qmake_path="$(readlink -f "$(command -v qmake)")"
        candidate="$(cd "$(dirname "$qmake_path")/.." && pwd)"
        if qt_root_is_compatible "$candidate"; then
            QT_ROOT="$candidate"
            return 0
        fi
    fi
    QT_ROOT=""
    return 1
}

if [[ -n "$REQUESTED_QT_ROOT" ]]; then
    qt_root_is_compatible "$REQUESTED_QT_ROOT" || \
        die "Qt root is not a complete Qt 5 installation: $REQUESTED_QT_ROOT"
else
    discover_qt_root || true
fi

if ((INSTALL_SYSTEM_PACKAGES)); then
    info "Installing missing system build dependencies"
    SUDO=()
    if ((EUID != 0)); then
        command -v sudo >/dev/null 2>&1 || die "sudo is required to install apt packages"
        SUDO=(sudo)
    fi

    apt_packages=(
        build-essential \
        ca-certificates \
        cmake \
        git \
        ninja-build \
        pkg-config \
        libgtk-3-dev \
        libjpeg-dev \
        libpng-dev \
        libtiff-dev \
        libavcodec-dev \
        libavformat-dev \
        libavutil-dev \
        libswscale-dev \
        libv4l-dev
    )
    if [[ -z "$QT_ROOT" ]]; then
        apt_packages+=(qt5-qmake qtbase5-dev qtbase5-dev-tools qtmultimedia5-dev)
    else
        info "Reusing existing Qt installation: $QT_ROOT"
    fi

    "${SUDO[@]}" apt-get update
    "${SUDO[@]}" apt-get install -y --no-install-recommends "${apt_packages[@]}"
fi

if [[ -z "$QT_ROOT" ]]; then
    discover_qt_root || die "a complete Qt 5 installation was not found after package installation"
fi

opencv_root_is_compatible() {
    local root="$1"
    local header="$root/include/opencv4/opencv2/core.hpp"
    local version_header="$root/include/opencv4/opencv2/core/version.hpp"
    local candidate library missing
    [[ -f "$header" && -f "$version_header" ]] || return 1
    grep -Eq '^#define CV_VERSION_MAJOR[[:space:]]+4$' "$version_header" || return 1
    for candidate in "$root/lib" "$root/lib64" "$root/lib/$(uname -m)-linux-gnu"; do
        missing=0
        for library in core imgproc imgcodecs highgui videoio; do
            if [[ ! -e "$candidate/libopencv_${library}.so" ]]; then
                missing=1
                break
            fi
        done
        ((missing == 0)) && return 0
    done
    return 1
}

if [[ -z "$OPENCV_ROOT" ]]; then
    for candidate in "$HOME/.local/opencv-${OPENCV_VERSION}" /usr /usr/local; do
        if opencv_root_is_compatible "$candidate"; then
            OPENCV_ROOT="$candidate"
            info "Reusing existing OpenCV installation: $OPENCV_ROOT"
            break
        fi
    done
fi
if [[ -z "$OPENCV_ROOT" ]]; then
    OPENCV_ROOT="$HOME/.local/opencv-${OPENCV_VERSION}"
fi

opencv_header="$OPENCV_ROOT/include/opencv4/opencv2/core.hpp"
opencv_lib_dir=""
for candidate in \
    "$OPENCV_ROOT/lib" \
    "$OPENCV_ROOT/lib64" \
    "$OPENCV_ROOT/lib/$(uname -m)-linux-gnu"; do
    missing_opencv_lib=0
    for library in core imgproc imgcodecs highgui videoio; do
        if [[ ! -e "$candidate/libopencv_${library}.so" ]]; then
            missing_opencv_lib=1
            break
        fi
    done
    if ((missing_opencv_lib == 0)); then
        opencv_lib_dir="$candidate"
        break
    fi
done

if [[ ! -f "$opencv_header" || -z "$opencv_lib_dir" ]]; then
    if ((BUILD_OPENCV)); then
        info "Building OpenCV ${OPENCV_VERSION} in $OPENCV_ROOT"
        work_dir="${TMPDIR:-/tmp}/qt-znxj-opencv-${OPENCV_VERSION}"
        source_dir="$work_dir/source"
        build_dir="$work_dir/build"
        mkdir -p "$work_dir"

        if [[ ! -d "$source_dir/.git" ]]; then
            rm -rf "$source_dir"
            git clone --branch "$OPENCV_VERSION" --depth 1 \
                https://github.com/opencv/opencv.git "$source_dir"
        fi

        cmake -S "$source_dir" -B "$build_dir" -G Ninja \
            -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_INSTALL_PREFIX="$OPENCV_ROOT" \
            -DBUILD_LIST=core,imgproc,imgcodecs,highgui,videoio \
            -DBUILD_EXAMPLES=OFF \
            -DBUILD_JAVA=OFF \
            -DBUILD_TESTS=OFF \
            -DBUILD_PERF_TESTS=OFF \
            -DBUILD_opencv_apps=OFF \
            -DBUILD_opencv_python2=OFF \
            -DBUILD_opencv_python3=OFF \
            -DWITH_GTK=ON \
            -DWITH_FFMPEG=ON
        cmake --build "$build_dir" --parallel "$JOBS"
        cmake --install "$build_dir"
    elif ((CHECK_ONLY)); then
        die "OpenCV not found under $OPENCV_ROOT"
    else
        die "OpenCV not found under $OPENCV_ROOT; remove --skip-opencv-build or pass --opencv-root"
    fi
fi

[[ -f "$opencv_header" ]] || die "OpenCV header is missing after installation: $opencv_header"

QMAKE="$QT_ROOT/bin/qmake"
[[ -x "$QMAKE" ]] || die "qmake is missing or not executable: $QMAKE"

halcon_root_is_compatible() {
    local root="$1"
    local version_header="$root/include/HVersNum.h"
    [[ -f "$root/include/HalconC.h" && -f "$version_header" ]] || return 1
    grep -Eq 'HLIB_MAJOR_NUM[[:space:]]+20' "$version_header" || return 1
    grep -Eq 'HLIB_MINOR_NUM[[:space:]]+11' "$version_header" || return 1
}

if [[ -n "$HALCON_ROOT" ]] && ! halcon_root_is_compatible "$HALCON_ROOT"; then
    info "Ignoring incompatible HALCON installation (required 20.11): $HALCON_ROOT"
    HALCON_ROOT=""
fi
if [[ -z "$HALCON_ROOT" ]]; then
    for candidate in /opt/halcon /opt/MVTec/HALCON* "$HOME"/HALCON*; do
        if halcon_root_is_compatible "$candidate"; then
            HALCON_ROOT="$candidate"
            info "Reusing existing HALCON installation: $HALCON_ROOT"
            break
        fi
    done
fi

[[ -n "$HALCON_ROOT" ]] || \
    die "compatible HALCON 20.11 was not found; install it and pass --halcon-root PATH"

[[ -f "$HALCON_ROOT/include/HalconC.h" ]] || \
    die "HALCON C API header is missing: $HALCON_ROOT/include/HalconC.h"

halcon_runtime=""
while IFS= read -r candidate; do
    halcon_runtime="$candidate"
    break
done < <(find "$HALCON_ROOT" -type f -name 'libhalcon.so*' -print 2>/dev/null)
[[ -n "$halcon_runtime" ]] || \
    die "HALCON runtime library (libhalcon.so*) was not found under $HALCON_ROOT"

if [[ -n "$HALCON_LICENSE" && "$HALCON_LICENSE" != *@* && ! -r "$HALCON_LICENSE" ]]; then
    info "Configured HALCON_LICENSE_FILE does not exist; searching under HALCONROOT"
    HALCON_LICENSE=""
fi
if [[ -z "$HALCON_LICENSE" ]]; then
    for license_dir in "$HALCON_ROOT/license" "$INITIAL_HALCON_ROOT/license"; do
        [[ -d "$license_dir" ]] || continue
        HALCON_LICENSE="$(find "$license_dir" -maxdepth 1 -type f \
            \( -name 'license*.dat' -o -name '*.lic' \) -print 2>/dev/null | sort -V | tail -n 1)"
        [[ -n "$HALCON_LICENSE" ]] && break
    done
fi

env_file="$SCRIPT_DIR/dependencies.env"
if ((!CHECK_ONLY)); then
    {
        printf '# Generated by install_dependencies.sh; load with: source %q\n' "$env_file"
        printf 'export OPENCV_ROOT=%q\n' "$OPENCV_ROOT"
        printf 'export QT_ROOT=%q\n' "$QT_ROOT"
        printf 'export HALCONROOT=%q\n' "$HALCON_ROOT"
        if [[ -n "$HALCON_LICENSE" ]]; then
            printf 'export HALCON_LICENSE_FILE=%q\n' "$HALCON_LICENSE"
        fi
        printf 'export PATH=%q:"$PATH"\n' "$QT_ROOT/bin"
    } >"$env_file"
fi

info "Dependency check passed"
"$QMAKE" -v
printf 'OpenCV root: %s\n' "$OPENCV_ROOT"
printf 'HALCON root: %s\n' "$HALCON_ROOT"
printf 'HALCON runtime: %s\n' "$halcon_runtime"
if [[ -n "$HALCON_LICENSE" ]]; then
    printf 'HALCON license: %s\n' "$HALCON_LICENSE"
else
    printf 'HALCON license: not explicitly configured (runtime validation still required)\n'
fi
printf '\nNext steps:\n'
if ((CHECK_ONLY)); then
    printf '  export HALCONROOT=%q\n' "$HALCON_ROOT"
    printf '  export PATH=%q:"$PATH"\n' "$QT_ROOT/bin"
else
    printf 'Environment file: %s\n' "$env_file"
    printf '  source %q\n' "$env_file"
fi
printf '  mkdir -p %q && cd %q\n' "$PROJECT_ROOT/build" "$PROJECT_ROOT/build"
printf '  %q %q\n' "$QMAKE" "$PROJECT_ROOT/qt_ui_test.pro"
printf '  make -j%s\n' "$JOBS"
