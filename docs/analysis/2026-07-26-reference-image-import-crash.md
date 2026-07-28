# 基准图 PC 导入后打开功能闪退分析

日期：2026-07-26

## 复现路径

登录 → 方案设置 → 方案 5221 → 基准图 → PC 导入 → 关闭文件对话框 → 功能。

## 调试器证据

旧的增量 Release 产物稳定触发：

```text
corrupted size vs. prev_size in fastbins
SIGABRT
```

多次运行的崩溃检测位置不固定，代表堆在更早阶段已经损坏。捕获到的代表性调用链包括：

```text
QArrayData::allocate
Ui_ReferenceImageDialog::setupUi
ReferenceImageDialog::ReferenceImageDialog
CameraParamsDialog::openReferenceImageDialog
```

```text
QJsonArray::insert
ToolResult::toJson
toolPreviewSnapshotToJson
SchemeStore::saveSchemeToFile
ReferenceImageDialog::openToolsDialog
```

另一次在 `cv::Mat::~Mat` → `SchemeStore::saveSchemeToFile` 检测到损坏。检测点横跨 Qt
分配器、JSON 和 OpenCV 析构，不能把其中任一检测点当作首次越界位置。

## 结论

问题来自标准构建目录中残留的旧 `*.o`、moc 和 ui 生成文件与当前源码混编，导致对象内存布局不一致。

证据：

- 原增量 Release 产物可重复崩溃。
- 全新 ASan 构建按相同路径运行，没有报告内存违规。
- 全新 Release 影子构建按相同路径连续运行 10 次，10 次均正常退出。
- 未改变基准图导入业务实现，仅完整清理并重编标准构建目录后，构建通过。

## 处理和验证

标准构建目录执行：

```bash
cd build/qt_ui_test
make clean
/home/tt/Qt/5.15.2/gcc_64/bin/qmake ../../qt_ui_test.pro \
  BUILD_ROOT="$PWD" CONFIG+=release CONFIG-=debug
make -j2
```

验证结果：

- 主工程全量 qmake/make：通过。
- 全新 Release 自动复现路径：连续 10 次通过。
- `reference_position_correction_private_model_smoke`：全部检查通过。
- 最终标准程序 offscreen 启动 5 秒保持运行，无异常退出。

调试复现期间使用的自动化入口和环境变量已全部移除，没有进入正式源码。
