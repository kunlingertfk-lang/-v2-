# GitHub 便携化构建 实现计划

> **面向 AI 代理的工作者：** 本计划可在批准后内联执行。步骤使用复选框 `- [ ]` 跟踪进度。

**目标：** 让项目 clone 到任意装好 Qt+OpenCV+HALCON 的机器后,设几个环境变量即可一键编译运行;并提供快速安装脚本;同时精简仓库供新建空 GitHub 仓库重推。

**架构：** 把硬编码在本机的 OpenCV/HALCON 路径全部改为环境变量驱动(`OPENCV_ROOT` / `HALCONROOT` / `HALCON_LICENSE_FILE`),保留 `local_paths.pri` 作为可选的每开发者覆盖;HALCON 仍是运行期 `dlopen`+`dlsym`,构建期只取头文件不链接;新增 `scripts/setup.sh` 做依赖检查+影子构建;`.gitignore` 排除 smoke/与训练数据,untrack 后推到新空仓库。

**技术栈：** Qt 5.15.2 (qmake) / OpenCV 4.x / HALCON 24.11 (运行期 dlopen) / bash

---

## 关键事实(已核实)

- HALCON 集成方式:源码 `#include <HalconC.h>`(只需头文件),函数通过 `dlopen("libhalconc.so")`+`dlsym` 调用,**不链接** `-lhalconc`。所以构建期只需 HALCON 头,运行期才需 `libhalconc.so`+license。
- 当前硬编码点:
  - `qt_ui_test.pro:17` `OPENCV_ROOT = /home/tt/.local/opencv-4.8.0`
  - `qt_ui_test.pro:21` HALCON 回退 `/home/tt/tfk/WorkerSpace/Software/HALCON-24.11.1.0-Progress-Steady`
  - `src/algorithms/halcon/HalconRuntimePaths.cpp:12-23` 硬编码 `/home/tt/...` 与 `/home/superhe/...`
- 本机环境:`HALCONROOT` 已设,`HALCON_LICENSE_FILE` 已设 → 改 `HalconRuntimePaths.cpp` 回退路径不影响本地运行(env 优先命中)。
- `rk_ai_materials/`(20MB,27 文件)在 src/ui/resources 中**无任何引用**,是纯训练数据,可排除。
- `smoke/`(21 文件)用户明确不上传。
- `AGENTS.md` 已被 `.gitignore`(`AGENTS.*`)排除,不会上传,无需改。
- 依赖获取:协作者均有 HALCON+license,走环境变量;OpenCV 支持 apt 系统安装或 `OPENCV_ROOT` 自编译两种。

## 文件清单

- 修改:`qt_ui_test.pro`(依赖路径段)
- 修改:`src/algorithms/halcon/HalconRuntimePaths.cpp`(去除机器路径,改 env 驱动)
- 修改:`.gitignore`(新增 `smoke/`、`local_paths.pri`、`rk_ai_materials/`)
- 新建:`README.md`(前置条件+环境变量+构建运行说明)
- 新建:`local_paths.pri.example`(每开发者覆盖模板)
- 新建:`scripts/setup.sh`(快速安装/检查/构建脚本)
- git 操作:`git rm --cached -r smoke/ rk_ai_materials/`(仅移出索引,保留本地)

---

### 任务 1:改造 `qt_ui_test.pro` 依赖路径段

**文件:** 修改 `qt_ui_test.pro:17-25` 及 `191-195`

- [ ] **步骤 1:替换 OpenCV+HALCON 路径段**

把 `qt_ui_test.pro` 第 17–25 行:
```qmake
OPENCV_ROOT = /home/tt/.local/opencv-4.8.0
INCLUDEPATH += $$OPENCV_ROOT/include/opencv4
HALCON_ROOT = $$(HALCONROOT)
!exists($$HALCON_ROOT/include/HalconC.h) {
    HALCON_ROOT = /home/tt/tfk/WorkerSpace/Software/HALCON-24.11.1.0-Progress-Steady
    message("Using bundled HALCON root: $$HALCON_ROOT")
}
INCLUDEPATH += $$HALCON_ROOT/include
message("HALCON root: $$HALCON_ROOT")
```
替换为:
```qmake
# 可选:每开发者可在 local_paths.pri(已 gitignore)中覆盖 OPENCV_ROOT / HALCON_ROOT
exists($$_PRO_FILE_PWD_/local_paths.pri): include($$_PRO_FILE_PWD_/local_paths.pri)

# OpenCV:设了 OPENCV_ROOT 用自编译安装(prefix+rpath);否则用系统安装(apt libopencv-dev,头文件在 /usr/include/opencv4)
OPENCV_ROOT = $$(OPENCV_ROOT)
!isEmpty(OPENCV_ROOT) {
    INCLUDEPATH += $$OPENCV_ROOT/include/opencv4
    LIBS += -L$$OPENCV_ROOT/lib -Wl,-rpath,$$OPENCV_ROOT/lib
} else {
    INCLUDEPATH += /usr/include/opencv4
    message("OPENCV_ROOT 未设置,使用系统 OpenCV (/usr/include/opencv4)")
}

# HALCON:必须设 HALCONROOT 指向 HALCON 安装目录(含 include/HalconC.h)。运行期 dlopen,无需链接 -lhalconc
HALCON_ROOT = $$(HALCONROOT)
isEmpty(HALCON_ROOT) {
    error("未设置环境变量 HALCONROOT。请 export HALCONROOT=<HALCON 安装目录>(含 include/HalconC.h),例如 /opt/halcon/24.11。详见 README.md。")
}
!exists($$HALCON_ROOT/include/HalconC.h) {
    error("HALCONROOT=$$HALCON_ROOT 下找不到 include/HalconC.h,请确认 HALCON 安装路径。详见 README.md。")
}
INCLUDEPATH += $$HALCON_ROOT/include
message("HALCON root: $$HALCON_ROOT")
```

- [ ] **步骤 2:确认 OpenCV 链接段无需改**

第 191–195 行原为:
```qmake
LIBS += -L$$OPENCV_ROOT/lib
LIBS += -Wl,-rpath,$$OPENCV_ROOT/lib
LIBS += -lopencv_core -lopencv_imgproc -lopencv_highgui -lopencv_videoio -lopencv_imgcodecs
LIBS += -ldl
```
把前两行(`-L$$OPENCV_ROOT/lib` 和 `-Wl,-rpath,...`)删除——它们已在上面的 `!isEmpty(OPENCV_ROOT)` 分支里按需添加;系统 OpenCV 分支不需要。保留:
```qmake
LIBS += -lopencv_core -lopencv_imgproc -lopencv_highgui -lopencv_videoio -lopencv_imgcodecs
LIBS += -ldl
```

- [ ] **步骤 3:验证本地构建**

运行:
```bash
cd /home/tt/tfk/WorkerSpace/project/V2_615_1628/qt_znxj_v2_work_1920_515
rm -rf build/verify_portable && mkdir -p build/verify_portable && cd build/verify_portable
/home/tt/Qt/5.15.2/gcc_64/bin/qmake ../../qt_ui_test.pro
make -j$(nproc)
```
预期:qmake 输出 `HALCON root: /home/tt/...`,`make` 成功产出 `qt_ui_test/bin/qt_ui_test`。

---

### 任务 2:`HalconRuntimePaths.cpp` 去机器路径,改 env 驱动

**文件:** 修改 `src/algorithms/halcon/HalconRuntimePaths.cpp:12-23, 84-122, 146-186`

- [ ] **步骤 1:删除硬编码常量,改为 env 派生**

把第 10–25 行匿名命名空间里的硬编码常量块:
```cpp
const QString kLocalHalconRoot =
        QStringLiteral("/home/tt/tfk/WorkerSpace/Software/HALCON-24.11.1.0-Progress-Steady");
const QString kLegacySuperheHalconRoot =
        QStringLiteral("/home/superhe/桌面/som-halcon/repository/packages.mvtec.com/halcon/halcon-24.11-progress-steady");
const QString kLegacySuperheRuntimeRoot =
        kLegacySuperheHalconRoot + QStringLiteral("/halcon-24.11.2.0-runtime-x64-linux");
const QString kLegacySuperheRuntimeGeneralRoot =
        kLegacySuperheHalconRoot + QStringLiteral("/halcon-24.11.2.0-runtime-general-x64-linux_aarch64-linux_armv7a-linux");
const QString kLegacySuperheLicenseRoot =
        kLegacySuperheRuntimeGeneralRoot + QStringLiteral("/license");
const QString kLocalHalconLicenseRoot =
        kLocalHalconRoot + QStringLiteral("/license");
const QString kOcrModelRelativePath =
        QStringLiteral("ocr/OCRB_0-9A-Z_NoRej.omc");
```
替换为:
```cpp
// HALCON 安装目录统一由环境变量 HALCONROOT 提供;license 与 OCR 模型都相对它定位。
const QString kOcrModelRelativePath =
        QStringLiteral("ocr/OCRB_0-9A-Z_NoRej.omc");

QString halconLicenseRootForEnv()
{
    const QString root = halconRootFromEnvironment();
    return root.isEmpty() ? QString() : cleanPath(root + QStringLiteral("/license"));
}
```
注意:`halconRootFromEnvironment()` 与 `cleanPath()` 已在同文件上方定义,顺序需保证 `halconLicenseRootForEnv` 在 `resolveBundledLicense` 之前。把 `halconRootFromEnvironment`/`cleanPath` 上移到该函数之前(它们当前在第 27/32 行,已在常量块之后——需要把这两个函数移到匿名命名空间顶部,常量块之前)。

- [ ] **步骤 2:`resolveBundledLicense` 改用 `$HALCONROOT/license`**

把 `resolveBundledLicense()`(原 84–122 行)中的 license 根列表:
```cpp
const QVector<QString> licenseRoots = {
    kLocalHalconLicenseRoot,
    kLegacySuperheLicenseRoot
};
```
替换为:
```cpp
const QString envLicenseRoot = halconLicenseRootForEnv();
const QVector<QString> licenseRoots = envLicenseRoot.isEmpty()
        ? QVector<QString>()
        : QVector<QString>{envLicenseRoot};
```
其余扫描逻辑(`license.dat`、`license*.dat`、preferred prefixes)保持不变。

- [ ] **步骤 3:`defaultHalconRoot` 改为纯 env**

把:
```cpp
QString defaultHalconRoot()
{
    const QString envRoot = halconRootFromEnvironment();
    return envRoot.isEmpty() ? kLocalHalconRoot : cleanPath(envRoot);
}
```
替换为:
```cpp
QString defaultHalconRoot()
{
    return cleanPath(halconRootFromEnvironment());
}
```

- [ ] **步骤 4:`halconLibCandidates` 去掉硬编码回退**

把:
```cpp
appendHalconLibCandidatesForRoot(&candidates, kLocalHalconRoot);
appendHalconLibCandidatesForRoot(&candidates, kLegacySuperheRuntimeRoot);
return candidates;
```
替换为:
```cpp
return candidates;
```
(`candidates` 此时已包含 explicitPath 与 env root 候选,足够。)

- [ ] **步骤 5:`ocrModelCandidates` 去掉硬编码回退**

把:
```cpp
appendOcrModelCandidateForRoot(&candidates, kLocalHalconRoot);
appendOcrModelCandidateForRoot(&candidates, kLegacySuperheRuntimeGeneralRoot);
return candidates;
```
替换为:
```cpp
return candidates;
```

- [ ] **步骤 6:验证无残留机器路径并构建**

运行:
```bash
grep -n "/home/tt\|/home/superhe" src/algorithms/halcon/HalconRuntimePaths.cpp   # 预期:无输出
cd build/verify_portable && make -j$(nproc)                                      # 预期:无重编译错误
```

---

### 任务 3:`.gitignore` 精简 + untrack

**文件:** 修改 `.gitignore`;git 操作 untrack

- [ ] **步骤 1:追加忽略规则**

在 `.gitignore` 末尾追加:
```gitignore

# 便携化:每开发者本地路径覆盖,不入库
local_paths.pri

# 不上传 smoke 子工程与 AI 训练数据(源码/数据,非构建运行所需)
smoke/
rk_ai_materials/
```

- [ ] **步骤 2:把已跟踪的 smoke/ 与 rk_ai_materials/ 移出索引(保留本地文件)**

```bash
git rm -r --cached smoke rk_ai_materials
git status   # 预期:smoke/、rk_ai_materials/ 显示为 deleted(仅索引)
```

---

### 任务 4:新增 `scripts/setup.sh` 快速安装脚本

**文件:** 新建 `scripts/setup.sh`(可执行)

- [ ] **步骤 1:写入脚本内容**

```bash
#!/usr/bin/env bash
# scripts/setup.sh — 依赖检查 + 影子构建一键脚本
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
    *) yellow "未知参数,忽略: $a" ;;
  esac
done

bold "===== 依赖检查 ====="
fail=0

# 1. Qt / qmake
QMAKE=""
if command -v qmake-qt5 >/dev/null 2>&1; then QMAKE=qmake-qt5
elif command -v qmake >/dev/null 2>&1; then QMAKE=qmake
elif [[ -n "${QTDIR:-}" && -x "$QTDIR/bin/qmake" ]]; then QMAKE="$QTDIR/bin/qmake"
elif [[ -x /opt/Qt/5.15.2/gcc_64/bin/qmake ]]; then QMAKE=/opt/Qt/5.15.2/gcc_64/bin/qmake
fi
if [[ -n "$QMAKE" ]]; then
  green "[OK] qmake: $($QMAKE -query QT_VERSION 2>/dev/null || echo '?')  ($QMAKE)"
else
  red "[缺] 未找到 qmake。安装 Qt 5.15 并把 bin 加入 PATH,或 export QTDIR=<Qt 安装目录>"
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
  red "[缺] 未找到 OpenCV。安装: sudo apt install libopencv-dev;或自编译后 export OPENCV_ROOT=<prefix>"
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
      yellow "[提示] 未设 HALCON_LICENSE_FILE,程序将自动使用: $lic"
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
  red "===== 依赖检查未通过,请按上面提示补齐后重试 ====="
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
```

- [ ] **步骤 2:赋予可执行权限并自测**

```bash
chmod +x scripts/setup.sh
./scripts/setup.sh            # 预期:四项 [OK],提示下一步
./scripts/setup.sh --build    # 预期:构建成功,产出 build/qt_ui_test/bin/qt_ui_test
```

---

### 任务 5:新增 `README.md` 与 `local_paths.pri.example`

**文件:** 新建 `README.md`、`local_paths.pri.example`

- [ ] **步骤 1:写 `README.md`**

````markdown
# 智能相机视觉工具 (qt_ui_test)

基于 Qt 的工业视觉工具配置与运行工程。视觉核心算法基于 HALCON,OpenCV 仅作图像容器/采集/格式桥接。

## 前置条件

| 依赖 | 版本 | 获取方式 |
|---|---|---|
| Qt | 5.15.x | Qt 在线安装器,或 `sudo apt install qtbase5-dev` |
| OpenCV | 4.x | `sudo apt install libopencv-dev`,或自编译后设 `OPENCV_ROOT` |
| HALCON | 24.11 | MVTec 官方安装 + 有效 license(商业软件,需自行获取) |

> HALCON 在本工程中是**运行期 dlopen** 加载,构建期只需头文件,运行期需要 `libhalconc.so` 与 license。

## 环境变量

```bash
export HALCONROOT=/opt/halcon/24.11                 # HALCON 安装目录(含 include/HalconC.h)
export HALCON_LICENSE_FILE=$HALCONROOT/license/license_xxx.dat   # 可选,不设则自动从 $HALCONROOT/license 查找
# 可选:自编译 OpenCV 时指定 prefix;不设则用系统 /usr/include/opencv4
# export OPENCV_ROOT=$HOME/.local/opencv-4.8.0
```

建议把上述 `export` 写入 `~/.bashrc` 或项目根的 `local_paths.pri`(见下)。

## 快速开始

```bash
git clone <repo> && cd <repo>
./scripts/setup.sh --run      # 检查依赖 + 构建 + 运行
```

或手动影子构建:

```bash
mkdir build && cd build
qmake ../qt_ui_test.pro
make -j$(nproc)
./qt_ui_test/bin/qt_ui_test
```

## 可选:local_paths.pri

不想用环境变量时,可复制 `local_paths.pri.example` 为 `local_paths.pri`(已 gitignore),在其中用 qmake 语法覆盖路径:

```bash
cp local_paths.pri.example local_paths.pri
# 编辑 local_paths.pri 后:
mkdir build && cd build && qmake ../qt_ui_test.pro && make -j$(nproc)
```

## 目录

- `src/` 业务代码、对话框、adapter、算法 runner
- `ui/` Qt Designer `.ui` 文件
- `resources/` 图标/字体/图片(运行需要)
- `styles/` QSS 样式
- `qt_ui_test.pro` 构建入口
- `scripts/setup.sh` 依赖检查+构建脚本
````

- [ ] **步骤 2:写 `local_paths.pri.example`**

```qmake
# 每开发者本地路径覆盖模板。
# 用法: cp local_paths.pri.example local_paths.pri  然后编辑。
# local_paths.pri 已被 .gitignore 忽略,不会入库。
# 取消下面相应行的注释并改成你本机的路径即可。

# OPENCV_ROOT = $$quote($$system(echo $HOME))/.local/opencv-4.8.0
# INCLUDEPATH += $$OPENCV_ROOT/include/opencv4
# LIBS += -L$$OPENCV_ROOT/lib -Wl,-rpath,$$OPENCV_ROOT/lib

# HALCON_ROOT = /opt/halcon/24.11
# INCLUDEPATH += $$HALCON_ROOT/include
```

---

### 任务 6:提交并推送到新空 GitHub 仓库

- [ ] **步骤 1:本地提交**

```bash
cd /home/tt/tfk/WorkerSpace/project/V2_615_1628/qt_znxj_v2_work_1920_515
git add qt_ui_test.pro src/algorithms/halcon/HalconRuntimePaths.cpp .gitignore README.md local_paths.pri.example scripts/setup.sh
git status   # 确认 smoke/、rk_ai_materials/ 在 deleted(索引)列表里,且 local_paths.pri 未被跟踪
git commit -m "build: 便携化依赖路径(env 驱动)+ 快速安装脚本 + 精简仓库

- qt_ui_test.pro: OPENCV_ROOT/HALCONROOT 改环境变量,去除硬编码 /home/tt 回退
- HalconRuntimePaths.cpp: 去除 /home/tt、/home/superhe 机器路径,license/OCR 改 $HALCONROOT 派生
- scripts/setup.sh: 依赖检查 + 影子构建一键脚本
- README.md / local_paths.pri.example: 构建运行说明
- .gitignore: 排除 smoke/、rk_ai_materials/、local_paths.pri
- untrack smoke/、rk_ai_materials/(保留本地)"
```

- [ ] **步骤 2:推送到新建空仓库(用户在 GitHub 先建空仓库)**

```bash
# 用户先在 GitHub 新建一个空仓库(不勾选 README/.gitignore),拿到地址后:
git remote set-url origin git@github.com:<用户>/<新仓库>.git
# 或新建一个 remote: git remote add github git@github.com:<用户>/<新仓库>.git && git push -u github main
git push -u origin main
```

因远端是空仓库,普通 push 即可,无需 --force。

---

## 自检

- **规格覆盖:** "下载后快速编译执行" → 任务 1/2 去硬编码 + 任务 4 setup 脚本 + 任务 5 README;"OpenCV+HALCON" → env 驱动 + dlopen 保留;"上传 GitHub" → 任务 3 精简 + 任务 6 推送。✓
- **占位符:** 无 TODO,脚本/代码均为完整内容。✓
- **本地行为不变:** env 已设,pro 走 env 分支,HalconRuntimePaths 走 env 命中,license 走 env。✓ 任务 1 步骤 3、任务 2 步骤 6 均有本地构建验证。
- **类型一致:** `halconLicenseRootForEnv` 在 `resolveBundledLicense` 前定义;`halconRootFromEnvironment`/`cleanPath` 上移到匿名命名空间顶部。✓
