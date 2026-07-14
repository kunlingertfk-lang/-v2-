# 注册分类检测训练窗口样式对齐设计

## 背景

`RegisteredClassificationDetectionTrainingDialog` 当前使用独立深灰 QSS，右侧卡片、目标列表、参数区和底部状态文字在截图中出现低对比、文字被深底吞没的问题。公共规范 `docs/FID/Function_Docs.md` 已明确训练和检测类二级窗口应参考 `RegisteredClassificationTrainingDialog` 的白底、高对比、蓝边、橙色主按钮风格。

## 范围

本次只修改 `src/RegisteredClassificationDetectionTrainingDialog.cpp` 的样式声明，不改交互逻辑、不改 `RegisteredClassificationTrainingDialog`，不抽公共样式 helper。

## 设计

- 检测训练窗口根样式对齐注册分类训练窗口：白底、深色文字、基础字号 `18px`。
- 左侧预览面板和右侧训练卡片使用白底、蓝色高可见边框和 `8px` 圆角。
- 右侧内容区使用白底，不再使用灰底或深底。
- 图像画布保持近黑底，外部工具栏和状态栏改为注册分类训练窗口同款浅色高对比样式。
- 目标列表表头、目标行、ROI 预览页、缩略图列表、按钮、下拉框和状态文字使用注册分类训练窗口同款 QSS 语义。
- `开始训练` 保持橙底白字主按钮。
- `请先添加注册图`、`请先标注目标`、`训练算法未接入` 等状态文案保持现有逻辑，仅通过样式提升可读性。

## 验收

- 右侧卡片和参数区不再出现深底深字。
- `目标列表`、`标签类型`、`目标总数/图像总数`、`Target`、`0 / 0`、两个开关文案和底部状态文字清晰可读。
- `矩形框选`、`多边形框选`、`开始训练` 按钮样式与注册分类训练窗口保持一致。
- 现有注册分类检测 smoke 通过。
- 主工程 qmake 和 make 通过。
