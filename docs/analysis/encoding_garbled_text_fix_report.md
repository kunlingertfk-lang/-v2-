# 乱码问题修复报告

> 日期：2026-06-22  
> 对应台账：`BUG-20260622-001 乱码问题`  
> 状态：代码侧已修复，待 Qt Creator 实机复核

## 1. 问题现象

运行日志里出现中文乱码。已观察到历史 `qt_ui_test.log` 中相机初始化失败文本被写成类似：

```text
æ— æ³•åˆå§‹åŒ–æ‘„åƒå¤´
```

该文本实际应为：

```text
无法初始化摄像头
```

## 2. 影响范围

本次按编码链路排查，风险点集中在：

- Qt message handler 写 stderr。
- `qt_ui_test.log` 日志文件写入。
- Qt Creator 的 Application Output。
- AI 远端 RKNN 桥接命令的 stdout/stderr。
- 包含中文的环境变量、路径或标签输出。

## 3. 根因

项目里存在多处依赖“本地编码”的转换：

```cpp
message.toLocal8Bit()
QString::fromLocal8Bit(...)
QTextStream stream(&file);
```

在 Linux/WSL/Qt Creator 环境中，如果进程 locale、终端编码或 Qt Creator 输出面板编码不一致，中文 UTF-8 字节可能被当成本地编码重复解释，最终写成乱码。

另外，qmake 工程没有显式声明 GCC 源码输入字符集和执行字符集。虽然当前文件是 UTF-8，但在不同 Kit 或环境下仍存在隐患。

## 4. 修复方案

### 4.1 固定 Qt 本地编码

在 `main()` 开始阶段设置：

```cpp
QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));
```

这样项目中仍保留的 `fromLocal8Bit()` / `toLocal8Bit()` 在 Qt5 下会按 UTF-8 处理，降低环境差异影响。

### 4.2 日志输出统一 UTF-8

`src/main.cpp` 中：

- stderr 输出由 `toLocal8Bit()` 改为 `toUtf8()`。
- `QTextStream` 写文件时显式 `stream.setCodec("UTF-8")`。

### 4.3 AI 远端桥接统一 UTF-8

`src/algorithms/ai/AiDetectionRunner.cpp` 和 `src/tooladapters/AiDetectionAdapter.cpp` 中：

- 环境变量读取改用 `QString::fromUtf8(qgetenv(...))`。
- 远端进程 stdout/stderr 改用 `QString::fromUtf8(...)`。
- OpenCV 写临时图片路径时改用 `toUtf8()`。

### 4.4 GCC 构建显式声明 UTF-8

`qt_ui_test.pro` 增加：

```qmake
gcc:QMAKE_CXXFLAGS += -finput-charset=UTF-8 -fexec-charset=UTF-8
```

确保 GCC 按 UTF-8 读取源码，并按 UTF-8 生成窄字符串字面量。

### 4.5 UI 中文字体兜底

Linux 环境中检查到：

```text
fc-match "Noto Sans CJK SC" -> DejaVu Sans
fc-match "Microsoft YaHei" -> DejaVu Sans
```

说明运行环境没有命中项目 QSS 中声明的中文字体。项目资源里已有：

```text
:/fonts/ALIMAMASHUHEITI-BOLD.OTF
```

但旧代码没有通过 `QFontDatabase::addApplicationFont()` 注册它，QSS 也没有把该字体加入 `font-family`。这会导致 UI 中文依赖系统字体回退，轻则显示方框/缺字，重则在部分环境中表现为“乱码”。

处理方式：

- 启动时加载内置字体 `ALIMAMASHUHEITI-BOLD.OTF`。
- 将该字体设置为应用默认字体。
- 在全局 QSS 和带局部 `font-family` 的 `.ui` 文件中加入 `"Alimama ShuHeiTi"`。

### 4.6 避免根目录旧 `ui_*.h` 覆盖 build 目录生成物

项目根目录残留了旧的 `ui_*.h`，这些文件属于 `.gitignore` 中声明的构建产物。qmake/g++ 曾经实际依赖到根目录旧 UI 头，例如：

```text
../../ui_LoginWindow.h
```

这会造成两个问题：

- `.ui` 文件更新后，build 目录中新生成的 UI 头没有被真正使用。
- 根目录旧 UI 头可能携带旧字体、旧文本或旧编码行为。

处理方式：

- `qt_ui_test.pro` 加入 `CONFIG += no_include_pwd`，避免项目根目录排在 build 目录前面。
- 将根目录旧 `ui_*.h` 非破坏性移动到 `backup/stale_generated_ui_headers_20260622/`。
- 重新 qmake 后确认 Makefile 不再依赖 `../../ui_*.h`。

## 5. 修改文件

```text
src/main.cpp
src/algorithms/ai/AiDetectionRunner.cpp
src/tooladapters/AiDetectionAdapter.cpp
qt_ui_test.pro
styles/app.qss
ui/CameraParamsDialog.ui
ui/OutputDialog.ui
ui/ReferenceImageDialog.ui
ui/ToolLibraryDialog.ui
docs/analysis/bug_fix_log.md
docs/analysis/encoding_garbled_text_fix_report.md
backup/stale_generated_ui_headers_20260622/
```

## 6. 验证结果

已执行：

```bash
cd /mnt/d/Share/WORK/project/V2_615_1628/V2_615_1628/qt_znxj_v2_work_1920_515/build/codex_run_release
HALCONROOT=/home/kunling/LIB/halcon24.11 /home/kunling/Qt/5.15.2/gcc_64/bin/qmake ../../qt_ui_test.pro CONFIG+=release
make -j$(nproc)
```

结果：

- 编译通过。
- 编译命令已包含 `-finput-charset=UTF-8 -fexec-charset=UTF-8`。
- `file -bi` 检查关键文件为 UTF-8。
- `grep -a "汇众智慧" qt_ui_test` 能在二进制中找到中文字符串。
- 应用短启动后仍可正常写入 `qt_ui_test.log`。

### 6.1 复测补充

2026-06-22 复测时发现，仅执行增量构建仍可能复用旧 `.o` 文件，导致 `MainWindow`、`CameraFrameProvider` 等旧目标文件中的中文字符串继续显示为乱码。

处理方式：

```bash
cd /mnt/d/Share/WORK/project/V2_615_1628/V2_615_1628/qt_znxj_v2_work_1920_515/build/codex_run_release
make clean
HALCONROOT=/home/kunling/LIB/halcon24.11 /home/kunling/Qt/5.15.2/gcc_64/bin/qmake ../../qt_ui_test.pro CONFIG+=release
make -j$(nproc)
```

干净重建后验证：

```bash
grep -a "无法初始化摄像头" qt_ui_test
grep -a "当前无图像" qt_ui_test
grep -a "æ—" qt_ui_test
```

结果：

```text
FOUND_EXPECTED_CAMERA_TEXT
FOUND_EXPECTED_NO_IMAGE_TEXT
NO_MOJIBAKE_BYTES
```

结论：代码侧乱码问题已解决；Qt Creator 中必须执行 `Clean Project` / `Rebuild Project`，不能只点普通 Build。

### 6.2 UI 乱码复测补充

UI 乱码继续排查后确认两类问题：

1. Linux 环境没有命中 QSS 中声明的 `Noto Sans CJK SC` / `Microsoft YaHei`，会退回 DejaVu Sans。
2. 根目录旧 `ui_*.h` 会干扰 build 目录中新生成的 UI 头。

修复后重新执行干净构建，验证：

```text
BUILD_EXIT=0
root-ui-deps 为空
ui_LoginWindow.h 依赖 build 目录生成物
```

运行短启动验证：

```text
[INFO] Loaded bundled Chinese font: "Alimama ShuHeiTi"
FOUND_ALIMAMA_FONT_STRING
```

结论：UI 字体侧已加载项目内置中文字体，构建侧已避免继续使用根目录旧 UI 生成头。

## 7. 注意事项

旧日志中已经写坏的乱码不会被本次代码修复自动还原。验证新行为时建议删除或归档旧 `qt_ui_test.log`，重新启动程序后观察新写入内容。

如果 Qt Creator 仍显示乱码，先确认是否执行过完整清理重建：

```text
Build -> Clean Project
Build -> Run qmake
Build -> Rebuild Project
```

如果后续发现 UI 控件文本仍乱码，应单独检查 `.ui` 文件编码、Qt Designer 保存编码、字体是否覆盖中文字符，以及 Qt Creator 文件编码设置。

## 8. 涉及知识点

- UTF-8 与本地编码的区别。
- Qt5 `QString`、`toUtf8()`、`toLocal8Bit()`、`fromUtf8()`、`fromLocal8Bit()`。
- `QTextStream::setCodec()` 对文件写入编码的影响。
- Linux locale 与 Qt Creator 输出面板编码。
- GCC `-finput-charset` 与 `-fexec-charset`。
- OpenCV 在 Linux 下路径参数通常按 UTF-8 字节传入。
