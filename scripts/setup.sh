#!/usr/bin/env bash
# scripts/setup.sh - 依赖检查 + 影子构建一键脚本
# 用法:
#   ./scripts/setup.sh            # 仅检查依赖
#   ./scripts/setup.sh --build    # 检查 + 构建
#   ./scripts/setup.sh --run      # 检查 + 构建 + 运行
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

red()    { printf '\033[31m%s\033[0m\n' "$*"; }
green()  { printf '\033[32m%s\033[0m\n' "$*"; }
yellow() { printf '\033[33m%s\033[0m\n' "$*"; }
bold()   { printf '\033[1m%s\033[0m\n' "$*"; }

BUILD=0; RUN=0
for a in "$@"; do
  case "$a" in
    --build) BUILD=1 ;;
    --run)   RUN=1 ;;
    -h|--help) sed -n '2,6p' "$0"; exit 0 ;;
    *) yellow "未知参数，忽略: $a" ;;
  esac
done

bold "===== 依赖检查 ====="
fail=0

# 1. Qt / qmake
QMAKE=""
if command -v qmake-qt5 >/dev/null 2>&1; then QMAKE=qmake-qt5
elif command -v qmake >/dev/null 2>&1; then QMAKE=qmake
elif [[ -n "${QTDIR:-}" && -x "$QTDIR/bin/qmake" ]]; then QMAKE="$QTDIR/bin/qmake"
elif [[ -n "${QTDIR:-}" && -x "$QTDIR/gcc_64/bin/qmake" ]]; then QMAKE="$QTDIR/gcc_64/bin/qmake"
elif [[ -x /opt/Qt/5.15.2/gcc_64/bin/qmake ]]; then QMAKE=/opt/Qt/5.15.2/gcc_64/bin/qmake
fi
if [[ -n "$QMAKE" ]]; then
  green "[OK] qmake: $($QMAKE -query QT_VERSION 2>/dev/null || echo '?')  ($QMAKE)"
else
  red "[缺] 未找到 qmake。安装 Qt 5.15 并把 bin 加入 PATH，或 export QTDIR=<Qt 安装目录>"
  fail=1
fi

# 2. OpenCV
if [[ -n "${OPENCV_ROOT:-}" ]]; then
  if [[ -f "$OPENCV_ROOT/include/opencv4/opencv2/core.hpp" ]]; then
    green "[OK] OpenCV (OPENCV_ROOT): $OPENCV_ROOT"
  else
    red "[缺] OPENCV_ROOT=$OPENCV_ROOT 下找不到 include/opencv4/opencv2/core.hpp"
    fail=1
  fi
elif [[ -f /usr/include/opencv4/opencv2/core.hpp ]]; then
  green "[OK] OpenCV (系统): /usr/include/opencv4"
else
  red "[缺] 未找到 OpenCV。安装: sudo apt install libopencv-dev；或自编译后 export OPENCV_ROOT=<prefix>"
  fail=1
fi

# 3. HALCON 头文件(构建需要)
if [[ -z "${HALCONROOT:-}" ]]; then
  red "[缺] 未设置 HALCONROOT。请 export HALCONROOT=<HALCON 安装目录> (含 include/HalconC.h)"
  fail=1
elif [[ ! -f "$HALCONROOT/include/HalconC.h" ]]; then
  red "[缺] HALCONROOT=$HALCONROOT 下找不到 include/HalconC.h"
  fail=1
else
  green "[OK] HALCON: $HALCONROOT"
fi

# 4. HALCON license(运行需要)
if [[ -z "${HALCON_LICENSE_FILE:-}" ]]; then
  if [[ -n "${HALCONROOT:-}" && -d "$HALCONROOT/license" ]]; then
    lic="$(find "$HALCONROOT/license" -maxdepth 1 \( -name 'license*.dat' -o -name 'license.dat' \) 2>/dev/null | head -1)"
    if [[ -n "$lic" ]]; then
      yellow "[提示] 未设 HALCON_LICENSE_FILE，程序将自动使用: $lic"
    else
      red "[缺] 未设 HALCON_LICENSE_FILE 且 $HALCONROOT/license 下无 license*.dat"
      fail=1
    fi
  fi
else
  if [[ -r "$HALCON_LICENSE_FILE" ]]; then
    green "[OK] HALCON license: $HALCON_LICENSE_FILE"
  else
    red "[缺] HALCON_LICENSE_FILE 不可读: $HALCON_LICENSE_FILE"
    fail=1
  fi
fi

if [[ $fail -ne 0 ]]; then
  echo
  red "===== 依赖检查未通过，请按上面提示补齐后重试 ====="
  exit 1
fi
green "===== 依赖检查通过 ====="

if [[ $BUILD -eq 0 && $RUN -eq 0 ]]; then
  echo
  bold "下一步: ./scripts/setup.sh --build   (或 --run 直接构建并运行)"
  exit 0
fi

# 影子构建
BUILD_DIR="$ROOT/build"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
bold "===== 构建 (qmake + make) ====="
"$QMAKE" "$ROOT/qt_ui_test.pro"
make -j"$(nproc)"
BIN="$BUILD_DIR/qt_ui_test/bin/qt_ui_test"
if [[ ! -x "$BIN" ]]; then
  red "构建失败或产物不存在: $BIN"
  exit 1
fi
green "构建成功: $BIN"

if [[ $RUN -eq 1 ]]; then
  bold "===== 运行 ====="
  exec "$BIN"
fi
