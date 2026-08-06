# 智能相机视觉工具 (qt_ui_test)

基于 Qt 的工业视觉工具配置与运行工程。视觉核心算法基于 HALCON，OpenCV 仅作图像容器/采集/格式桥接。

## 前置条件

| 依赖 | 版本 | 获取方式 |
|---|---|---|
| Qt | 5.15.x | Qt 在线安装器，或 `sudo apt install qtbase5-dev` |
| OpenCV | 4.x | `sudo apt install libopencv-dev`，或自编译后设 `OPENCV_ROOT` |
| HALCON | 24.11 | MVTec 官方安装 + 有效 license（商业软件，需自行获取） |

> HALCON 在本工程中是**运行期 dlopen** 加载：构建期只需头文件 `HalconC.h`，运行期才需要 `libhalconc.so` 与 license。

## 环境变量

```bash
export HALCONROOT=/opt/halcon/24.11                                       # HALCON 安装目录(含 include/HalconC.h)
export HALCON_LICENSE_FILE=$HALCONROOT/license/license_xxx.dat            # 可选，不设则自动从 $HALCONROOT/license 查找
# 可选:自编译 OpenCV 时指定 prefix；不设则用系统 /usr/include/opencv4
# export OPENCV_ROOT=$HOME/.local/opencv-4.8.0
```

建议把上述 `export` 写入 `~/.bashrc`，或改用项目根的 `local_paths.pri`（见下）。

## 快速开始

```bash
git clone <repo> && cd <repo>
./scripts/setup.sh --run      # 检查依赖 + 构建 + 运行
```

或手动影子构建：

```bash
mkdir build && cd build
qmake ../qt_ui_test.pro
make -j$(nproc)
./qt_ui_test/bin/qt_ui_test
```

## 可选: local_paths.pri

不想用环境变量时，可复制 `local_paths.pri.example` 为 `local_paths.pri`（已 gitignore），在其中用 qmake 语法覆盖路径：

```bash
cp local_paths.pri.example local_paths.pri
# 编辑 local_paths.pri 后:
mkdir build && cd build && qmake ../qt_ui_test.pro && make -j$(nproc)
```

## 目录

- `src/` 业务代码、对话框、adapter、算法 runner
- `ui/` Qt Designer `.ui` 文件
- `resources/` 图标/字体/图片（运行需要）
- `styles/` QSS 样式
- `qt_ui_test.pro` 构建入口
- `scripts/setup.sh` 依赖检查 + 构建脚本
