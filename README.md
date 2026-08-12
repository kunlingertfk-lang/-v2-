# 智能相机视觉工具 (qt_ui_test)

基于 Qt 的工业视觉工具配置与运行工程。视觉核心算法基于 HALCON，OpenCV 仅作图像容器/采集/格式桥接。

团队分支管理、GitLab MR、历史迁移、回滚和提交边界统一遵循 [`GIT_WORKFLOW.md`](GIT_WORKFLOW.md)。

## 前置条件

| 依赖 | 版本 | 获取方式 |
|---|---|---|
| Qt | 5.15.x | Qt 在线安装器，或 `sudo apt install qtbase5-dev` |
| OpenCV | 4.x | `sudo apt install libopencv-dev`，或自编译后设 `OPENCV_ROOT` |
| HALCON | 20.11 | MVTec 官方安装 + 有效 license（商业软件，需自行获取） |

> HALCON 在本工程中是**运行期 dlopen** 加载：构建期只需头文件 `HalconC.h`，运行期才需要 `libhalconc.so` 与 license。

## 环境变量

```bash
export HALCONROOT=/opt/halcon                                             # HALCON 20.11 安装目录(含 include/HalconC.h)
export HALCON_LICENSE_FILE=$HALCONROOT/license/license_xxx.dat            # 可选，不设则自动从 $HALCONROOT/license 查找
# 可选:自编译 OpenCV 时指定 prefix；不设则用系统 /usr/include/opencv4
# export OPENCV_ROOT=$HOME/.local/opencv-4.8.0
```

建议把上述 `export` 写入 `~/.bashrc`，或改用项目根的 `local_paths.pri`（见下）。

完整的新环境安装、权限问题和 License 配置见 [`INSTALLATION.md`](INSTALLATION.md)。

## 快速开始

```bash
git clone <repo> && cd <repo>
./scripts/install_dependencies.sh --halcon-root /opt/halcon
source scripts/dependencies.env
./scripts/setup.sh --run      # 检查依赖 + 构建 + 运行
```

## 编译过程

### 1. 加载依赖环境

每次打开新终端后，先在项目根目录加载安装脚本生成的环境文件：

```bash
cd /path/to/project
source scripts/dependencies.env
```

确认 qmake 和依赖路径：

```bash
"$QT_ROOT/bin/qmake" -v
echo "OpenCV: $OPENCV_ROOT"
echo "HALCON:  $HALCONROOT"
echo "License: ${HALCON_LICENSE_FILE:-未显式配置}"
```

未配置 HALCON License 不影响 qmake 和编译，但运行 HALCON 算子时需要有效授权。

### 2. 生成 Makefile

使用 `build/` 做影子构建，避免 Makefile、对象文件和 Qt 生成文件散落到源码目录：

```bash
mkdir -p build
cd build
"$QT_ROOT/bin/qmake" ../qt_ui_test.pro
```

qmake 会读取：

- `qt_ui_test.pro`：主工程入口和源码清单。
- `qmake/opencv.pri`：OpenCV 头文件、库目录和链接配置。
- `qmake/halcon_20_11.pri`：HALCON 20.11 头文件和版本校验。
- `local_paths.pri`：可选的开发者本地路径覆盖。

### 3. 编译

```bash
make -j"$(nproc)"
```

主要输出目录：

```text
build/qt_ui_test/obj/    C++ 对象文件
build/qt_ui_test/moc/    Qt moc 生成文件
build/qt_ui_test/ui/     Qt uic 生成头文件
build/qt_ui_test/rcc/    Qt 资源生成文件
build/qt_ui_test/bin/    最终可执行文件
```

编译成功后的程序为：

```text
build/qt_ui_test/bin/qt_ui_test
```

### 4. 运行

仍在 `build/` 目录时：

```bash
./qt_ui_test/bin/qt_ui_test
```

或者在项目根目录运行：

```bash
./build/qt_ui_test/bin/qt_ui_test
```

### 5. 增量编译

修改源码后通常不需要重新执行 qmake，直接在 `build/` 中运行：

```bash
make -j"$(nproc)"
```

修改以下内容后建议重新执行 qmake：

- `qt_ui_test.pro`
- `qmake/*.pri`
- 新增或删除 `.cpp`、`.h`、`.ui`、资源文件
- Qt、OpenCV 或 HALCON 路径发生变化

```bash
cd build
"$QT_ROOT/bin/qmake" ../qt_ui_test.pro
make -j"$(nproc)"
```

### 6. 完整清理后重编译

在项目根目录执行：

```bash
rm -rf build
mkdir -p build
cd build
"$QT_ROOT/bin/qmake" ../qt_ui_test.pro
make -j"$(nproc)"
```

`build/` 中均为可重新生成的构建产物，删除不会影响源码。

### 7. 使用一键脚本

完成依赖安装并加载 `dependencies.env` 后，也可以执行：

```bash
./scripts/setup.sh --build    # 检查依赖并构建
./scripts/setup.sh --run      # 检查、构建并运行
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
- `scripts/install_dependencies.sh` 依赖安装与兼容性检查脚本
