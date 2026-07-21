# 开发环境与依赖安装

本文说明如何在新的 Ubuntu/Debian 环境中安装、检查并配置本项目依赖，以及如何处理常见的权限、路径和 HALCON License 问题。

## 1. 依赖要求

| 依赖 | 项目要求 | 安装策略 |
|---|---|---|
| 操作系统 | Ubuntu 22.04+ 或兼容 Debian 系统 | 脚本使用 `apt-get` 安装系统包 |
| 编译工具 | GCC/G++、make、CMake、Ninja、pkg-config | 脚本自动安装缺失包 |
| Qt | 完整 Qt 5，含 Widgets、Concurrent、Multimedia、MultimediaWidgets | 优先复用兼容安装，否则通过 apt 安装 Qt 5 |
| OpenCV | OpenCV 4.x | 优先复用兼容安装，否则编译安装 OpenCV 4.8.0 |
| HALCON | **HALCON 20.11** | 商业软件，必须使用 MVTec 安装介质手动安装 |
| HALCON License | 本地 License 文件、License Server 或其他有效授权 | 脚本只发现和记录，不提供授权 |

> 视觉算子的核心算法依赖 HALCON。OpenCV 仅用于图像容器、采集、显示和格式桥接。

## 2. 重要注意事项

不要使用下面的方式运行完整脚本：

```bash
sudo bash scripts/install_dependencies.sh
```

完整脚本应由普通用户运行。脚本只在安装 apt 软件包时自行调用 `sudo`；OpenCV 会以当前用户身份安装到用户目录。整段脚本使用 sudo 可能在 `/tmp`、`$HOME/.local` 或项目目录中留下 root 所有的文件。

正确方式：

```bash
./scripts/install_dependencies.sh --halcon-root /opt/halcon
```

## 3. 安装 HALCON 20.11

HALCON 无法由本脚本下载。请先使用 MVTec 官方安装介质安装 HALCON 20.11，推荐安装目录：

```text
/opt/halcon
```

检查头文件和版本：

```bash
test -f /opt/halcon/include/HalconC.h
grep -E 'HLIB_(MAJOR|MINOR|REVISION)_NUM' /opt/halcon/include/HVersNum.h
```

输出应包含主版本 `20` 和次版本 `11`。检查运行库：

```bash
find /opt/halcon -type f -name 'libhalcon.so*'
```

如 HALCON 安装在其他目录，后续通过 `--halcon-root` 指定实际路径。

### 3.1 配置 License

使用本地 License 文件：

```bash
export HALCON_LICENSE_FILE=/path/to/license.dat
```

使用 License Server：

```bash
export HALCON_LICENSE_FILE=27000@license-server
```

检查本地 License 文件：

```bash
test -r "$HALCON_LICENSE_FILE" && echo "HALCON License 文件可读"
```

## 4. 执行依赖安装脚本

进入项目根目录：

```bash
cd /path/to/project
chmod +x scripts/install_dependencies.sh
```

建议先做只读检查：

```bash
./scripts/install_dependencies.sh \
  --check-only \
  --halcon-root /opt/halcon
```

新环境中缺少依赖时执行完整安装：

```bash
./scripts/install_dependencies.sh \
  --halcon-root /opt/halcon
```

脚本会依次：

1. 检查操作系统和参数。
2. 检查并复用完整的 Qt 5。
3. 使用 apt 补齐编译工具和必要开发包。
4. 检查已有 OpenCV 4.x。
5. 缺少兼容 OpenCV 时下载、编译并安装 OpenCV 4.8.0。
6. 检查 HALCON 20.11 头文件和运行库。
7. 发现本地 License 文件或保留 License Server 配置。
8. 生成 `scripts/dependencies.env`。

### 4.1 常用参数

| 参数 | 作用 |
|---|---|
| `--check-only` | 只检查，不安装、不编译、不生成环境文件 |
| `--skip-system` | 跳过 apt，仍允许检查或编译 OpenCV |
| `--skip-opencv-build` | OpenCV 缺失时直接报错，不从源码编译 |
| `--opencv-root PATH` | 指定已有 OpenCV 4 安装前缀 |
| `--qt-root PATH` | 指定 Qt 5 安装根目录，目录下应有 `bin/qmake` |
| `--halcon-root PATH` | 指定 HALCON 20.11 安装根目录 |
| `--jobs N` | 指定 OpenCV 和工程编译并发数 |

已有系统包全部安装完成时，可跳过 apt：

```bash
./scripts/install_dependencies.sh \
  --skip-system \
  --halcon-root /opt/halcon
```

## 5. 依赖复用规则

### Qt

脚本只复用 Qt 5，并检查以下模块：

- Qt Widgets
- Qt Concurrent
- Qt Multimedia
- Qt MultimediaWidgets

例如 Ubuntu 24.04 提供的 Qt 5.15.13 可以使用，不要求必须为 Qt 5.15.2。

```bash
/usr/bin/qmake -v
```

看到下面的提示属于正常复用：

```text
Reusing existing Qt installation: /usr
```

### OpenCV

脚本优先检查：

```text
$HOME/.local/opencv-4.8.0
/usr
/usr/local
```

找到兼容 OpenCV 4.x 及所需库时直接复用；否则安装 OpenCV 4.8.0 到：

```text
$HOME/.local/opencv-4.8.0
```

项目 qmake 配置同时支持 `lib`、`lib64`、`lib/x86_64-linux-gnu` 和 `lib/aarch64-linux-gnu`。

### HALCON

本分支要求 HALCON 20.11。检测到其他 HALCON 版本时不会混用，而是继续寻找兼容安装；没有兼容安装时停止并报错。

## 6. 加载环境配置

安装成功后执行：

```bash
source scripts/dependencies.env
```

该文件通常包含：

```bash
export OPENCV_ROOT=/home/user/.local/opencv-4.8.0
export QT_ROOT=/usr
export HALCONROOT=/opt/halcon
export HALCON_LICENSE_FILE=/path/to/license.dat
export PATH=/usr/bin:"$PATH"
```

环境变量只对当前终端有效。每次打开新终端后需要重新 `source`，或者在 IDE 的构建环境中加载该文件。

检查配置：

```bash
echo "$QT_ROOT"
echo "$OPENCV_ROOT"
echo "$HALCONROOT"
echo "$HALCON_LICENSE_FILE"
"$QT_ROOT/bin/qmake" -v
```

`dependencies.env` 含本机路径，已由 `.gitignore` 排除，不应提交。

## 7. 构建项目

推荐使用影子构建：

```bash
cd /path/to/project
source scripts/dependencies.env

mkdir -p build
cd build
"$QT_ROOT/bin/qmake" ../qt_ui_test.pro
make -j"$(nproc)"
```

运行主程序：

```bash
./qt_ui_test/bin/qt_ui_test
```

也可以使用项目检查/构建脚本：

```bash
./scripts/setup.sh --build
./scripts/setup.sh --run
```

## 8. 常见问题

### 8.1 OpenCV 编译出现 Permission denied

典型错误：

```text
/tmp/qt-znxj-opencv-4.8.0/build/CMakeDownloadLog.txt: Permission denied
```

原因通常是之前使用 sudo 运行完整脚本，临时目录属于 root。处理方法：

```bash
sudo rm -rf /tmp/qt-znxj-opencv-4.8.0
sudo chown -R "$USER:$USER" "$HOME/.local" 2>/dev/null || true

./scripts/install_dependencies.sh --halcon-root /opt/halcon
```

重新执行时不要在完整脚本前加 sudo。

### 8.2 HALCON was not found

检查实际安装位置：

```bash
find /opt "$HOME" -type f -path '*/include/HalconC.h' 2>/dev/null
```

找到后传入包含 `include/HalconC.h` 的根目录：

```bash
./scripts/install_dependencies.sh --halcon-root /actual/halcon/root
```

### 8.3 HALCON 版本不兼容

检查版本：

```bash
grep -E 'HLIB_(MAJOR|MINOR)_NUM' /actual/halcon/root/include/HVersNum.h
```

本分支必须是 20.11。其他版本即使存在 `HalconC.h`，qmake 也会拒绝构建。

### 8.4 License 路径过期

检查当前配置：

```bash
echo "$HALCON_LICENSE_FILE"
test -r "$HALCON_LICENSE_FILE" || echo "License 路径不可读"
```

查找实际文件并重新设置：

```bash
find "$HALCONROOT/license" -maxdepth 1 -type f \
  \( -name 'license*.dat' -o -name '*.lic' \)

export HALCON_LICENSE_FILE=/actual/license/file.dat
```

### 8.5 GitHub 下载失败

OpenCV 源码默认从 GitHub 下载。网络不可用时可选择：

1. 配置可访问 GitHub 的网络。
2. 预先安装兼容的系统 OpenCV 4。
3. 在可联网机器准备 OpenCV 安装目录，再通过 `--opencv-root` 指定。

系统 OpenCV 示例：

```bash
sudo apt update
sudo apt install libopencv-dev
pkg-config --modversion opencv4
```

### 8.6 新终端找不到依赖

重新加载：

```bash
source /path/to/project/scripts/dependencies.env
```

## 9. 清理构建产物

`build/`、`build_verify*`、Makefile、对象文件和可执行文件都是可重新生成的构建产物，不应提交到 Git。

清理普通影子构建目录：

```bash
rm -rf build
```

重新构建即可恢复。不要删除 `src/`、`ui/`、`resources/`、`qmake/` 或 `scripts/` 中的源码和配置文件。
