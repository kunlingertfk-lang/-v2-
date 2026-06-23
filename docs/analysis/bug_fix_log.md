# 项目 Bug 记录与修复知识库

> 维护规则：今后每个 bug 都在本文档追加一条记录。记录必须覆盖现象、影响范围、复现步骤、定位流程、根因、解决方案、验证方式和涉及到的技术知识。  
> 更新时间：2026-06-22

## 记录模板

```text
## BUG-YYYYMMDD-NNN 标题

- 状态：待定位 / 定位中 / 已修复 / 已验证 / 暂缓
- 发现日期：
- 发现环境：
- 影响范围：
- 现象描述：
- 复现步骤：
- 期望结果：
- 实际结果：
- 初步判断：
- 定位流程：
- 根因结论：
- 解决方案：
- 修改文件：
- 验证流程：
- 回归风险：
- 涉及知识点：
- 后续建议：
```

## BUG-20260622-001 乱码问题

- 状态：已修复，已通过干净重建和启动日志验证；待用户在 Qt Creator 中目视复核 UI
- 发现日期：2026-06-22
- 发现环境：Linux Qt Creator / WSL 运行环境
- 影响范围：运行日志、Qt Creator Application Output、Qt UI 中文显示、AI 远端桥接 stdout/stderr、包含中文的环境变量或临时路径
- 现象描述：运行日志中出现中文乱码，例如历史日志里“无法初始化摄像头”被写成 `æ— æ³•åˆå§‹åŒ–æ‘„åƒå¤´` 形式
- 复现步骤：
  1. 在 Linux/WSL Qt Creator 环境中启动程序。
  2. 触发相机初始化失败或其它中文告警。
  3. 查看 Application Output 或 `qt_ui_test.log`。
- 期望结果：中文、英文、路径、日志和 UI 文本均应按正确编码显示
- 实际结果：历史版本通过本地编码写日志和进程输出，locale 不一致时中文被错误转码
- 初步判断：
  - 如果乱码出现在 Windows PowerShell 调用 WSL 的输出中，优先检查 PowerShell 编码、WSL 输出编码和命令输出是否混合 UTF-16/UTF-8。
  - 如果乱码出现在 Qt Creator 的 Compile Output / Application Output，优先检查系统 locale、Qt Creator 文本编码、源码文件编码和运行环境变量。
  - 如果乱码出现在应用 UI，优先检查源码字符串、`.ui` 文件、`.qss`、资源文件和字体加载。
  - 如果乱码出现在日志文件 `qt_ui_test.log`，优先检查 `QTextStream` 写入编码、日志查看器编码和源码字符串编码。
- 定位流程：
  1. 明确乱码出现位置：应用 UI、Qt Creator 输出面板、终端、日志文件、配置文件还是文档。
  2. 记录原始乱码截图或复制原文，同时记录期望显示文本。
  3. 检查系统编码环境：
     ```bash
     locale
     echo $LANG
     echo $LC_ALL
     ```
  4. 检查源码和资源文件编码，重点看 `.cpp`、`.h`、`.ui`、`.qss`、`.json` 是否统一为 UTF-8。
  5. 如果是 Qt UI 文本乱码，检查 `QStringLiteral`、`QString::fromUtf8`、`QTextCodec` 兼容逻辑和字体是否覆盖中文字符。
  6. 如果是日志乱码，检查 `src/main.cpp` 中 `QTextStream` 输出编码，以及查看日志的软件是否以 UTF-8 打开。
  7. 如果是 PowerShell/WSL 输出乱码，分别在 WSL 原生终端、Qt Creator Application Output、Windows Terminal 中对比同一命令输出。
- 根因结论：
  - `src/main.cpp` 中 Qt message handler 使用 `message.toLocal8Bit()` 写 stderr，依赖系统 locale。
  - `QTextStream` 写 `qt_ui_test.log` 时未显式指定 UTF-8，依赖 Qt 本地编码。
  - AI 远端桥接把 stdout/stderr 和环境变量按 `fromLocal8Bit()` 解析，远端脚本或标签输出为 UTF-8 时可能被错误解码。
  - qmake 工程未显式声明 GCC 源码输入字符集和执行字符集，Qt Creator/系统环境变化时存在隐患。
  - Linux 环境没有安装/命中 QSS 中声明的 `Noto Sans CJK SC` / `Microsoft YaHei`，Qt 字体回退到 DejaVu Sans，中文 UI 可能缺字或显示异常。
  - 项目根目录残留旧 `ui_*.h` 构建产物，qmake/g++ 曾依赖到 `../../ui_LoginWindow.h` 这类旧文件，导致 build 目录中新生成的 UI 头未必被实际使用。
- 解决方案：
  - 程序启动时设置 Qt locale codec 为 UTF-8。
  - stderr 日志输出改为 `QString::toUtf8()`。
  - `QTextStream` 写日志文件时显式 `setCodec("UTF-8")`。
  - AI 桥接 stdout/stderr、环境变量读取和本地图片路径字节转换改为 UTF-8。
  - qmake 增加 GCC 编码参数：`-finput-charset=UTF-8 -fexec-charset=UTF-8`。
  - 对已有构建目录必须执行 clean/rebuild，避免旧 `.o` 文件继续携带错误编码字符串。
  - 启动时用 `QFontDatabase::addApplicationFont()` 加载内置中文字体 `ALIMAMASHUHEITI-BOLD.OTF`，并设置为应用默认字体。
  - 全局 QSS 和带局部字体族的 `.ui` 文件加入 `"Alimama ShuHeiTi"`。
  - `qt_ui_test.pro` 增加 `no_include_pwd`，并将根目录旧 `ui_*.h` 移动到 `backup/stale_generated_ui_headers_20260622/`，避免旧 UI 生成头覆盖 build 输出。
- 修改文件：
  - `src/main.cpp`
  - `src/algorithms/ai/AiDetectionRunner.cpp`
  - `src/tooladapters/AiDetectionAdapter.cpp`
  - `qt_ui_test.pro`
  - `styles/app.qss`
  - `ui/CameraParamsDialog.ui`
  - `ui/OutputDialog.ui`
  - `ui/ReferenceImageDialog.ui`
  - `ui/ToolLibraryDialog.ui`
  - `backup/stale_generated_ui_headers_20260622/`
- 独立报告：`docs/analysis/encoding_garbled_text_fix_report.md`
- 验证流程：
  1. 使用 WSL Qt 5.15.2 重新 qmake + make，构建通过。
  2. 构建命令中确认出现 `-finput-charset=UTF-8 -fexec-charset=UTF-8`。
  3. `file -bi` 确认关键源码和文档为 `charset=utf-8`。
  4. `grep -a "汇众智慧" qt_ui_test` 确认中文字符串以 UTF-8 保留进二进制。
  5. 短启动应用，确认 `qt_ui_test.log` 仍能正常写入。
  6. 首次增量构建复测仍出现旧乱码，确认原因是旧 `.o` 被复用。
  7. 执行 `make clean` 后完整重建，再检查二进制：能找到“无法初始化摄像头”“当前无图像”，且找不到旧 `æ—` mojibake 字节。
  8. UI 修复后重新 qmake + clean build，确认 `root-ui-deps` 为空，Makefile 不再依赖 `../../ui_*.h`。
  9. 启动应用，日志确认 `Loaded bundled Chinese font: "Alimama ShuHeiTi"`。
  10. 用户需要在 Qt Creator 中执行 `Clean Project` / `Run qmake` / `Rebuild Project`，再目视复核登录页、主界面、工具配置页和 Application Output。
- 回归风险：
  - 修改编码相关逻辑可能影响历史配置文件读取。
  - 修改字体可能改变 UI 排版。
  - 修改 locale 只影响当前机器，不等于项目已具备跨机器稳定性。
- 涉及知识点：
  - UTF-8、GBK、UTF-16 等字符编码差异。
  - Linux locale 与 `LANG` / `LC_ALL`。
  - Qt 字符串类型：`QString`、`QStringLiteral`、`QString::fromUtf8`。
  - Qt Designer `.ui` 文件编码。
  - Qt 日志输出、`QTextStream` 和文件查看器编码。
  - WSL 与 Windows 终端之间的输出转码差异。
- 后续建议：
  - 旧 `qt_ui_test.log` 中已经写坏的历史乱码不会自动恢复；需要重新生成日志或人工删除旧日志后验证新日志。
  - 修改编译编码参数后，普通增量 Build 可能不重编全部源码；必须 Clean/Rebuild。
  - 如果 Qt UI 控件仍乱码，继续检查 `.ui` 文件编码和字体覆盖，不要和日志乱码混为同一问题。
  - 后续所有 bug 均先登记本文档，再进入代码修改和验证。
