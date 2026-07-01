# 颜色识别功能实现记录

## 目标范围

本文档单独记录颜色识别算子的当前实现方案、已实现功能、具体实现细节、涉及文件更改，以及后续用户新提出的需求。后续继续开发颜色识别时，以 `AGENTS.md` 的 HALCON 前置规则为最高约束，以本文档作为颜色识别功能状态和需求追踪依据。

## 实现方案

### 总体链路

- 颜色识别从旧的 OpenCV HSV 阈值覆盖率方案，改为 `ColorRecognitionHalconRunner` 的 HALCON 直方图特征方案。
- `ColorRecognitionDialog` 负责主配置界面，包括模板训练、检测区域、结果判断、右侧预览和测试运行。
- `ColorTemplateDialog` 负责独立模板创建/编辑，包括模板名、标签、ROI 样本、特征类型、高级参数和样本图像来源。
- `ColorRecognitionAdapter` 从 `ToolConfig.params` 和 `judgeRule` 解析模板、样本、检测 ROI、判断规则，然后调用 `ColorRecognitionHalconRunner`。
- `ToolsDialog` 负责工具列表页的颜色识别工具配置入口和工具页预览，工具页预览不得显示配置页残留的交互 ROI。

### 实现原理

- 配置数据以颜色模板为核心保存：主界面只管理模板列表和当前检测 ROI；模板创建窗口负责标签、样本 ROI、样本特征和高级参数。
- `ColorTemplateDialog` 采用无 UI 文件的代码构建方式，窗口顶部深色标题栏由 `headerFrame` 子控件绘制。
- 模板创建窗口拖动通过两层事件处理实现：
  - 保留 `ColorTemplateDialog::mousePressEvent()` / `mouseMoveEvent()` / `mouseReleaseEvent()`，用于对话框自身顶部约 44px 区域收到鼠标事件时拖动窗口。
  - 对顶部 `headerFrame` 和标题文字安装 `eventFilter()`，并通过 `colorTemplateDragHandle=true` 属性标识拖动热区；鼠标按下时记录全局坐标和窗口左上角，鼠标移动时调用 `move(startFramePos + currentGlobalPos - startGlobalPos)` 移动窗口，鼠标释放时结束拖动。
  - 关闭按钮不安装拖动热区属性，因此关闭按钮点击不参与窗口拖动。
  - 拖动逻辑只存在于 `ColorTemplateDialog`，不修改 `ToolsDialog` 的窗口拖动行为。
- 样本添加时，`ColorTemplateDialog::addSampleFromCurrentRoi()` 从当前图像和矩形样本 ROI 生成训练样本：
  - 调用 `ColorRecognitionHalconRunner::extractFeature()` 提取直方图特征。
  - 同时裁剪当前 ROI 图像，编码为 PNG base64 保存到样本字段，用于模板回显、导入导出和主界面 ROI 缩略图展示。
- 测试运行时，`ColorRecognitionDialog` 将当前模板、检测 ROI 和判断规则写入 `ToolConfig`，由 `ColorRecognitionAdapter` 转换为 HALCON runner 配置并执行识别。
- 工具页预览只展示识别结果，不展示配置阶段的交互 ROI；颜色识别结果中的配置 ROI overlay 会在工具页预览链路中过滤。

### HALCON 算法约束

- 核心颜色识别算法必须使用 HALCON，不允许恢复 OpenCV HSV 覆盖率作为核心识别路径。
- 当前第一阶段只实现 `直方图特征`。
- `色谱特征` 只保留配置枚举和 UI 入口，运行时返回 `unsupported_feature`，不得崩溃。
- 图像输入仍可使用 `cv::Mat` 作为工程载体，但进入 runner 后必须桥接为 HALCON 图像。
- 当前直方图链路包含：HALCON 图像桥接、ROI region/domain、通道拆分、颜色空间转换、直方图提取、直方图交集相似度比较流程。

### HALCON 算子链

- runtime 入口：
  - 继续使用 `HalconRuntimePaths` 解析 HALCON runtime 路径。
  - `ColorRecognitionHalconRunner` 使用 `dlopen/dlsym` 解析 HALCON C 接口符号，缺少 runtime、license 或必要符号时返回明确错误，不 fallback 到 OpenCV 核心算法。
- 图像与 ROI 特征链：
  - `gen_image_interleaved`：将工程侧 `cv::Mat` BGR 图像桥接为 HALCON image。
  - `decompose3`：拆分三通道图像。
  - `trans_from_rgb`：将三通道图像转换到 `hsv` 空间，得到 H/S/V 通道。
  - `gen_rectangle1`：根据归一化 ROI 转换后的像素矩形生成 HALCON 矩形 region。
  - `reduce_domain`：将 H/S/V 单通道图像限制到 ROI domain。
  - `gray_histo_range`：分别对 H、S 以及可选 V 通道提取固定 bin 数直方图。
  - 直方图结果按通道内像素总数归一化后拼接为特征向量；`brightnessEnabled=false` 时只使用 H/S，开启时使用 H/S/V。
- 直方图交集相似度链：
  - `tuple_min2`：对当前 ROI 特征和模板样本特征逐 bin 取最小值。
  - `tuple_sum`：分别计算交集向量、当前 ROI 特征和模板样本特征的总和。
  - 相似度公式为 `similarity = sum(min(queryFeature, sampleFeature)) / min(sum(queryFeature), sum(sampleFeature))`。
  - 遍历当前模板下所有同维度样本，选择 `similarity` 最大的样本作为匹配样本。
  - 匹配样本的 `classId` 作为预测类别 id，匹配样本或 labels 中的名称作为预测类别。
- 得分映射：
  - 当前 `rating` 即直方图交集相似度，范围按 0-1 归一化。
  - 输出 `score = clamp(similarity * 100, 0, 100)`。
  - payload 写入 `scoreDirection=higher_is_better`、`scoreFormula=histogram_intersection_similarity_x100`、`ratingMode=histogram_intersection_similarity`。

### 算法实现原理

- 颜色识别第一阶段使用“颜色直方图特征 + HALCON tuple 交集相似度”的方式实现。
- 训练样本生成：
  - 用户在 `ColorTemplateDialog` 中选择标签，并在样本图像上框选矩形 ROI。
  - 系统把 ROI 归一化坐标转换为当前图像上的像素矩形。
  - runner 将输入图像统一为 8-bit BGR 三通道；灰度图会桥接为三通道，四通道图会转为 BGR。
  - 图像进入 HALCON 后转为 HSV 空间，只在 ROI domain 内统计颜色分布。
  - 每个样本保存一条特征向量和对应 `classId`；特征向量来自 H/S 或 H/S/V 通道直方图拼接。
- 直方图特征：
  - `sensitivity=low` 时每个通道 8 个 bin。
  - `sensitivity=medium` 时每个通道 16 个 bin。
  - `sensitivity=high` 时每个通道 32 个 bin。
  - `brightnessEnabled=false` 时特征为 H + S 两个通道直方图。
  - `brightnessEnabled=true` 时特征为 H + S + V 三个通道直方图。
  - 每个通道的直方图会按该通道 ROI 内像素总数归一化，减少 ROI 面积变化对比较结果的影响。
- 识别运行：
  - `ColorRecognitionAdapter` 从当前激活模板中读取 labels、samples、featureType、sensitivity、brightnessEnabled、knnK 和判断规则。
  - runner 对当前检测 ROI 提取一条查询特征。
  - runner 只使用与查询特征维度一致的模板样本；若没有可用样本，返回 `invalid_model_samples`。
  - runner 使用 HALCON `tuple_min2` 和 `tuple_sum` 逐样本计算直方图交集相似度。
  - `knnK` 和 `knnDistance` 字段保留为历史配置字段，新版直方图交集判定不使用 KNN 分类器。

### 结果比较与判定

- 旧版需求：结果与判断基于 HALCON `classify_class_knn` 的预测类别和 `rating`。
- 新版需求：结果与判断改为基于直方图交集相似度，不再使用 `classify_class_knn` 作为分类判定。
- 对每个模板样本，runner 计算：
  - `intersection = sum(min(queryFeature, sampleFeature))`。
  - `normalizer = min(sum(queryFeature), sum(sampleFeature))`。
  - `similarity = normalizer > 0 ? intersection / normalizer : 0`。
- runner 选择 `similarity` 最大的样本作为最佳匹配。
- runner 将 `similarity` 转成界面分数：
  - `score = clamp(similarity * 100, 0, 100)`。
  - 分数越高表示当前 ROI 颜色直方图与最佳模板样本越接近。
- 类别名称解析：
  - 优先用预测 `classId` 在模板 labels 中查找标签名。
  - 如果 labels 中没有找到，再从可用样本中按 `classId` 回退查找样本标签。
- OK/NG 判定有两种模式：
  - `最低分数`：`score >= minScore` 判定 OK，否则 NG。
  - `类别判断`：`predictedLabel == expectedLabel` 判定 OK，否则 NG；此时分数仍输出，但不参与 OK/NG。
- 结果输出包含：
  - `predictedLabel`、`predictedClassId`。
  - `similarity` / `rating` 原始值和转换后的 `score`。
  - `scoreDirection=higher_is_better`。
  - `comparisonMethod=histogram_intersection`。
  - `sampleCount`、`featureLength`、`roiPixelsRect`、`elapsedMs`。
  - ROI 矩形 overlay 和 OK/NG 文本 overlay。

### UI 方案

- 主窗口 `ColorRecognitionDialog` 采用三张截图的颜色识别框架：
  - 左侧浅色参数区。
  - 顶部 `基础 / 全部` 分段切换。
  - 左侧卡片包含 `模板训练 / 检测区域 / 结果判断`。
  - 右侧深色图像预览区，显示基准图/当前图像、ROI overlay、状态栏、测试结果。
- 模板创建窗口 `ColorTemplateDialog` 采用模板截图结构：
  - 顶部深色标题栏。
  - 左侧浅色参数栏。
  - 右侧深色图像画布。
  - 底部显示 ROI 状态，右下角 `取消 / 完成`。
- 控件样式参考现有非颜色识别 dlg：
  - 配置卡片白底深字。
  - 下拉框白底黑字。
  - 按钮分主动作橙色、普通白底灰边、禁用态弱化。
  - ROI 工具按钮使用互斥按钮组。

## 已实现功能

### 主颜色识别对话框

- 已接入 `ColorRecognitionDialog` 作为颜色识别工具配置入口。
- 已支持 `基础 / 全部` 分段切换骨架。
- 已实现模板训练区：
  - 模板列表。
  - 当前模板摘要。
  - `添加模板`。
  - `编辑`。
  - `重命名`。
  - `删除`。
  - `导入模板`。
  - `导出模板`。
- 已支持模板 `.bin` 导入/导出，内容为当前颜色模板 JSON 数据。
- 已实现结果判断：
  - `最低分数`。
  - `类别判断`。
  - 类别下拉项来自当前选中模板标签。
- 已实现检测区域：
  - 全局检测入口。
  - 矩形 ROI 检测入口。
  - 圆形按钮当前按矩形 ROI 行为处理，保持与用户要求“与矩形一致”的临时策略。
- 已实现右侧预览：
  - 显示基准图或当前帧。
  - 显示 ROI overlay。
  - 显示 OK/NG、类别、分数、样本数和状态文本。
- 已实现 `测试运行` 的开始/停止式连续检测：
  - 点击 `测试运行` 后启动定时检测。
  - 按钮文字切换为停止状态。
  - 再次点击停止检测并恢复按钮文字。

### 模板创建对话框

- 已新增独立 `ColorTemplateDialog`。
- 已支持模板名编辑。
- 已支持特征类型选择：
  - `直方图特征` 可用。
  - `色谱特征` 保留但不实现。
- 已支持标签管理：
  - 添加标签。
  - 重命名标签。
  - 删除标签。
  - 标签行显示样本数量。
- 已支持图像来源：
  - `添加当前图像`。
  - `添加图片`，从 PC 导入图片。
- 已支持矩形 ROI 样本：
  - 只保留矩形 ROI。
  - 添加 ROI 样本。
  - 删除当前 ROI 样本。
  - ROI 状态栏显示归一化坐标。
- 已支持高级参数：
  - 敏感度。
  - 亮度是否参与直方图。
  - K 值。
  - KNN 距离保留为 HALCON 默认策略。
- 已修复 `添加 ROI 样本` 点击导致程序退出的主要问题。
- 已修复下拉框和输入控件白底黑字可读性问题。
- 已实现模板创建窗口按父窗口自适应打开，比父窗口小一圈，避免控件堆叠。

### ROI 样本保存与显示

- ROI 样本已不再只保存坐标。
- 每个样本增加保存 ROI 图像数据：
  - `roiImagePngBase64`
  - `roiImageWidth`
  - `roiImageHeight`
- 添加 ROI 样本时，会从当前样本图像中裁剪 ROI，编码为 PNG base64，保存到模板样本。
- 模板导入/导出会携带 ROI 图像数据。
- 重新打开配置后，样本数量和样本 ROI 图像数据可回显。
- 当前模板创建对话框内的 ROI 样本列表可显示固定尺寸缩略图。

### 主界面模板列表

- 主界面模板训练列表已改为层级结构：
  - 模板行。
  - 标签行。
  - ROI 样本行。
- 模板行可显示首个样本 ROI 图像作为图标。
- ROI 样本行已支持使用保存的 ROI 图像作为缩略图。
- 点击 ROI 样本行时，可在右侧预览区显示对应 ROI overlay。
- 再次点击同一个 ROI 样本时，可隐藏对应 ROI overlay。
- 当前实现仍存在用户指出的视觉问题：主界面模板列表中 ROI 样本仍有 `ROI 1` 文字行，后续需要改为更接近截图要求的纯图像 ROI 样本展示。

### 工具页 ROI 残留处理

- `ToolsDialog` 在工具页显示颜色识别预览时，已过滤颜色识别结果中的 `ROI` overlay。
- `ToolsDialog` 切换工具预览时会调用 `clearRoi()`，清理配置窗口残留的交互 ROI。
- 目标效果：在工具页面预览颜色识别工具时，图像上不应显示用于配置的 ROI 框。

## 具体实现细节

### 配置字段

当前颜色识别配置以模板列表模式保存，核心字段包括：

- `params.colorModel.activeTemplateId`：当前激活模板 id。
- `params.colorModel.templates[]`：模板列表。
- `templateId`：稳定模板 id。
- `name`：模板名称。
- `featureType`：`histogram` 或 `spectrum`。
- `labels[]`：标签列表，包含名称和 class id。
- `samples[]`：样本列表。
- `samples[].label`：样本标签。
- `samples[].classId`：样本类别 id。
- `samples[].feature`：已提取的直方图特征向量。
- `samples[].roiNormalized`：样本 ROI 归一化坐标。
- `samples[].roiImagePngBase64`：样本 ROI 图片 PNG base64。
- `samples[].roiImageWidth`：样本 ROI 图片宽度。
- `samples[].roiImageHeight`：样本 ROI 图片高度。
- `sensitivity`：敏感度。
- `brightnessEnabled`：亮度是否参与。
- `knnK`：历史 KNN 配置字段，新版直方图交集判定不使用。
- `knnDistance`：历史 KNN 距离策略字段，新版直方图交集判定不使用，payload 中记录为 `not_used_histogram_intersection`。
- `judgeRule.mode`：`min_score` 或 `category`。
- `judgeRule.minScore`：最低分数。
- `judgeRule.expectedLabel`：目标类别。

### 测试运行流程

- `ColorRecognitionDialog::runTest()` 作为测试运行按钮入口。
- 首次点击启动连续检测：
  - 设置运行状态。
  - 立即执行一次 `performTestRun()`。
  - 启动 `QTimer`。
- 再次点击停止连续检测：
  - 停止 `QTimer`。
  - 恢复按钮文案。
- `performTestRun()`：
  - 获取当前预览图或基准图。
  - 生成当前 `ToolConfig`。
  - 调用 `ColorRecognitionAdapter`。
  - 展示 `ToolResult`。

### 样本 ROI 图像保存流程

- 在 `ColorTemplateDialog::addSampleFromCurrentRoi()` 中：
  - 获取当前矩形 ROI。
  - 用 HALCON runner 提取该 ROI 特征。
  - 从当前样本显示图裁剪 ROI 图片。
  - 编码为 PNG base64。
  - 写入 `ColorRecognitionSampleData`。
  - 刷新标签数量、样本列表和状态栏。

### ROI overlay 区分

- 模板 ROI：
  - 发生在 `ColorTemplateDialog`。
  - 用于训练样本。
  - 保存到模板样本。
- 检测 ROI：
  - 发生在 `ColorRecognitionDialog` 主界面。
  - 用于测试运行和实际识别。
  - 保存到颜色识别工具参数。
- 工具页预览：
  - 只显示工具结果必要 overlay。
  - 不显示配置过程中的交互 ROI。

## 文件更改记录

### 新增文件

- `src/ColorTemplateDialog.h`
- `src/ColorTemplateDialog.cpp`
- `docs/analysis/color_recognition_function_implementation.md`

### 主要修改文件

- `AGENTS.md`
  - 增加 HALCON 算子强制前置规则。
  - 更新颜色识别 UI 和算法规划。
- `qt_ui_test.pro`
  - 加入 `ColorTemplateDialog.*`。
  - 加入 `ColorRecognitionHalconRunner.*`。
- `ui/ColorRecognitionDialog.ui`
  - 重制颜色识别主界面骨架。
  - 增加模板管理、检测区域、结果判断和预览相关控件。
- `src/ColorRecognitionDialog.h`
  - 增加模板列表、连续测试、ROI 显隐、模板导入导出等状态和槽函数。
- `src/ColorRecognitionDialog.cpp`
  - 实现模板列表模式。
  - 实现模板 JSON 序列化/反序列化。
  - 实现 `.bin` 模板导入导出。
  - 实现测试运行定时器。
  - 实现主界面 ROI 操作和 ROI 样本点击显示。
  - 实现 ROI 图片字段保存与读取。
- `src/ColorTemplateDialog.h`
  - 定义 `ColorRecognitionTemplateData`、`ColorRecognitionLabelData`、`ColorRecognitionSampleData`。
  - 增加 ROI 图片保存字段。
- `src/ColorTemplateDialog.cpp`
  - 实现模板创建独立对话框。
  - 实现标签、样本、图像来源、ROI 样本、缩略图、参数设置。
- `src/tooladapters/ColorRecognitionAdapter.cpp`
  - 解析模板列表配置。
  - 调用 `ColorRecognitionHalconRunner`。
  - 映射 HALCON 结果到 `ToolResult`。
- `src/tooladapters/ColorRecognitionAdapter.h`
  - 持有 `ColorRecognitionHalconRunner`。
- `src/algorithms/recognition/ColorRecognitionHalconRunner.h`
  - 定义 HALCON 颜色识别配置、标签、样本、特征结果、运行结果。
- `src/algorithms/recognition/ColorRecognitionHalconRunner.cpp`
  - 实现 HALCON 直方图特征提取。
  - 实现 HALCON `tuple_min2` / `tuple_sum` 直方图交集相似度识别。
  - 实现错误状态和结果输出。
- `src/ToolsDialog.cpp`
  - 接入颜色识别配置对话框。
  - 过滤工具页颜色识别 ROI overlay。
  - 清理工具页残留交互 ROI。

## 已验证情况

- 已通过 `git diff --check`。
- 已通过 `/home/tt/Qt/5.15.2/gcc_64/bin/qmake qt_ui_test.pro`。
- 已通过 `make -j$(nproc)`。
- 已通过 `QT_QPA_PLATFORM=offscreen ./qt_ui_test` 启动检查。
- offscreen 启动能解析 HALCON license。
- offscreen 启动只能确认程序能启动，不能替代 GUI 手动点击验收。

## 当前已知问题与新需求

### 1. 模板创建窗口需要可拖动

用户最新确认需求：

- `创建颜色模板 / 颜色模板创建` 窗口需要支持鼠标拖动移动。
- 拖动区域应主要是顶部标题栏或深色标题区域。
- 当前实现已存在鼠标事件相关函数，但仍需按实际截图验证拖动手感和可拖动区域是否符合要求。

### 2. 主界面模板训练区需要图像式 ROI 展示

用户最新确认需求：

- 主界面模板训练列表不能以纯文字形式显示 ROI，例如 `ROI 1`。
- 应显示模板训练中的 ROI 图像缩略图。
- 标签下方应展示该标签对应的所有 ROI 图像。
- ROI 图像应来自保存的样本 ROI 图片，而不是根据当前图像坐标重新裁剪。
- 当前实现已经保存 ROI 图像并在列表 item 图标中使用，但仍保留 `ROI 1` 文字行；后续需要改为更接近截图的图像网格/图像块展示。

### 3. 测试运行中检测 ROI 仍需可拖动

用户最新确认需求：

- 点击 `测试运行` 后，用户仍然可以拖动和修改检测 ROI。
- 不能出现测试运行开始后无法进行 ROI 区域选择的问题。
- ROI 修改后，后续检测应立即或下一轮使用最新 ROI。

### 4. 测试运行需要防阻塞方案

用户最新确认需求：

- 测试运行不能阻塞 UI。
- 不能因为 HALCON 检测正在运行，导致 ROI 选择、拖动、按钮点击无响应。
- 第一阶段建议方案：
  - 使用定时器触发检测。
  - 增加 `m_testRunBusy` 之类的运行中标记，上一轮未结束时跳过下一轮，防止重入。
  - ROI 拖动只更新状态，检测函数每轮读取最新 ROI。
- 如果 HALCON 检测耗时明显，后续升级方案：
  - 检测移到 worker 线程。
  - UI 线程只负责 ROI 交互、参数更新和结果刷新。
  - 结果回传时检查序号，避免旧结果覆盖新 ROI 的结果。

### 5. 模板 ROI 与检测 ROI 必须严格区分

用户最新确认需求：

- 模板创建窗口中的 ROI 是样本 ROI，只用于训练。
- 主颜色识别窗口中的 ROI 是检测 ROI，只用于测试运行和实际识别。
- 点击模板样本 ROI 可以用于预览显示/隐藏，但不应无意覆盖主检测 ROI。
- 后续实现需要检查当前 `m_displayedSampleIndex`、`m_roiNormalized`、`m_globalDetection` 的交互边界，避免状态串扰。

### 6. 工具页不显示 ROI

用户需求：

- 在工具页面时，图像上不应该有 ROI。
- 当前已增加工具页预览过滤和 `clearRoi()`，但仍需要 GUI 里手动确认：
  - 从颜色识别配置页返回工具页。
  - 切换工具页选中颜色识别工具。
  - 图像上不应残留橙色配置 ROI。

## 后续建议执行顺序

1. 先完善 `ColorTemplateDialog` 拖动行为，确保标题栏拖动稳定。
2. 重做主界面模板训练列表的 ROI 展示控件，从 `QListWidget` 文字层级改为更接近截图的图像块/缩略图区域。
3. 明确 ROI 图像块点击行为：
   - 单击样本 ROI：右侧显示/隐藏该样本 ROI overlay。
   - 单击模板或标签：只改变选中态，不覆盖检测 ROI。
4. 修改测试运行交互：
   - 运行中保持检测 ROI 可拖动。
   - 增加检测防重入标记。
   - ROI 拖动后下一轮检测使用最新 ROI。
5. 若 UI 仍卡顿，再把测试检测切到异步 worker。
6. 最后做 GUI 手动验收：
   - 添加模板。
   - 添加当前图像。
   - 添加图片。
   - 添加 ROI 样本。
   - 删除当前 ROI。
   - 保存后重开。
   - 主界面模板区查看 ROI 缩略图。
   - 测试运行中拖动 ROI。
   - 返回工具页确认无 ROI 残留。

## 2026-06-29 最新需求实现记录

### 已实现

- 模板创建窗口拖动：
  - `ColorTemplateDialog` 保持顶部标题区域拖动窗口的实现。
  - 鼠标按住窗口顶部约 44px 范围后可移动对话框。
- 主界面模板训练区 ROI 图像展示：
  - `ColorRecognitionDialog::updateTemplateList()` 中，ROI 样本 item 不再显示 `ROI 1` 文字。
  - ROI 样本 item 使用已保存的 `roiImagePngBase64` 生成缩略图。
  - 标签和模板文字仍保留，用于表达模板/标签层级；ROI 样本本身改为图像块展示。
  - ROI 样本 tooltip 保留 `标签名 ROI 序号`，用于鼠标悬停确认归属。
- 测试运行防阻塞：
  - `测试运行` 不再在 UI 线程同步执行 HALCON 检测。
  - 使用 `QFutureWatcher<ToolResult>` + `QtConcurrent::run()` 后台执行单轮颜色识别。
  - 增加 `m_testRunBusy` 防重入标记，上一轮检测未完成时跳过下一次定时触发。
  - 增加 `m_testRunGeneration`，停止测试或重新开始后，旧一轮异步结果不会覆盖当前状态。
- 测试运行中检测 ROI 可继续拖动：
  - 测试运行启动时清除样本 ROI 预览状态，回到检测 ROI。
  - `displayResult()` 在连续测试且非全局检测时，不再关闭 ROI drawing。
  - 连续测试中结果 overlay 会过滤 `ROI` 框，保留交互式检测 ROI 作为唯一可拖动检测框。
  - ROI 拖动后更新 `m_roiNormalized`，下一轮异步检测读取最新 `toToolConfig()`，即使用最新 ROI。
- 模板 ROI 与检测 ROI 区分：
  - 主界面 ROI 样本点击只设置 `m_displayedSampleIndex` 用于预览样本 ROI。
  - 点击样本 ROI 不写入 `m_roiNormalized`，因此不会覆盖检测 ROI。
  - 启动测试运行会清空 `m_displayedSampleIndex`，避免样本 ROI 预览状态参与检测。
- 关闭安全：
  - `ColorRecognitionDialog` 析构时停止连续测试。
  - 若后台测试任务仍在运行，等待其结束后再释放 UI，避免异步结果回调访问已销毁对象。

### 涉及文件

- `src/ColorRecognitionDialog.h`
  - 新增 `QFutureWatcher<ToolResult>`。
  - 新增 `m_testRunBusy`。
  - 新增 `m_testRunGeneration`。
- `src/ColorRecognitionDialog.cpp`
  - 引入 `QtConcurrent`。
  - 将 `performTestRun()` 改为异步后台运行。
  - 修改 `displayResult()`，连续测试时保持检测 ROI 可编辑。
  - 修改 `updateTemplateList()`，ROI 样本从文字行改为图像缩略 item。
  - 修改 `runTest()`，启动测试时退出样本 ROI 预览状态。

### 验证记录

- 已执行 `git diff --check -- src/ColorRecognitionDialog.cpp src/ColorRecognitionDialog.h`，无格式错误。
- 已执行 `make -j$(nproc)`，编译和链接通过。
- 仍需 GUI 手动验收：
  - 模板创建窗口顶部拖动。
  - 主界面模板训练区 ROI 样本是否为图像展示。
  - 测试运行中拖动检测 ROI 是否顺畅。
  - 拖动 ROI 后检测结果是否按新 ROI 更新。
  - 点击样本 ROI 是否只预览，不覆盖检测 ROI。



 ### 实现功能
 - `ColorTemplateDialog`这个对话框实现拖动，其余不能进行更改
 - `ColorTemplateDialog`中的 `样本数量`显示白底黑字
 - 主界面模板训练列表的 ROI 展示控件，下方中部显示标签，要有个方框将roi和标签框起来，便于查看

### 2026-06-29 FID 细化需求实现记录

- `ColorTemplateDialog` 拖动范围保持在模板创建对话框自身顶部标题区域，未修改 `ToolsDialog` 拖动行为。
- `ColorTemplateDialog` 的 `样本数量` 标签已明确设置为白底、黑字、浅灰边框，并居中显示。
- `ColorRecognitionDialog` 主界面模板训练列表中的 ROI 样本已改为自定义卡片：
  - 卡片整体为白底浅灰边框。
  - 上方显示保存的 ROI 缩略图。
  - 下方中部显示该 ROI 所属标签。
  - 缩略图区域保留橙色边框，便于快速查看 ROI 图像。


  ## 未实现功能（目前先不用管）
  - `ColorTemplateDialog` 拖动问题已按标题栏 `eventFilter()` 方案修复；仍需在真实 GUI 中手动复验拖动手感。
 
### 已实现功能
- 更改结果比较与判断显示的字体，ok为绿色，NG为红色，字体加大加粗；roi太小就放在roi外面不能被边框边界遮挡。roi能放下字体就处于roi正中心
- `ColorTemplateDialog`中的折叠也要与父窗口一致，可以折叠和显示参数
- 亮度参数勾选进行优化，要有边框。
- 屏蔽区域，改为“编辑”按钮。点击显示 选择“多边形roi图标”和“完成”按钮。当点击一次图标，可以在视图区域进行绘制多边形，点击一下左键增加一个角点，双击完成后能进行拖动，每个角点上增加一个透明小方框可以进行拖动。点击完成按钮，回到 “编辑”按钮。在编辑期间，不能选择检测区域的roi进行绘制。



### 
- 更改屏蔽roi区域，如果检测区域与屏蔽区域有交叉，完全包含，与完全被包含。需要进行裁剪，只检测没被屏蔽的区域。如果完全被包含在屏蔽区域中，则显示棕色颜色识别字体，不用进行检测。
- 屏蔽roi在完成确认后，自动隐藏，当编辑屏蔽roi时，在显示出来

### 2026-06-30 最新需求实现记录

- 屏蔽 ROI 已接入颜色识别 runner：
  - 主界面保存的 `detectMaskPolygon` 会经 `ColorRecognitionAdapter` 传递到 `ColorRecognitionHalconRunner`。
  - runner 使用 HALCON `gen_region_polygon_filled` 生成屏蔽多边形 region。
  - runner 使用 HALCON `difference` 对检测矩形 ROI 与屏蔽 region 做差集，后续直方图只在差集后的有效检测 region 内统计。
  - runner 使用 HALCON `area_center` 检查有效检测 region 面积。
- 检测 ROI 与屏蔽 ROI 的关系处理：
  - 有交叉时，检测区域会扣除屏蔽区域，只检测剩余区域。
  - 屏蔽区域包含检测区域时，有效检测面积为 0，runner 返回 `masked_roi_empty`，不继续进行颜色直方图比较。
  - 检测区域包含屏蔽区域时，会裁剪掉内部屏蔽部分，继续检测剩余区域。
- 完全屏蔽时显示：
  - 输出 `颜色识别 已屏蔽` 文本 overlay。
  - overlay 使用 `status=MASKED`，显示层映射为棕色字体。
  - payload 写入 `detectMaskApplied=true`、`detectMaskFullyCoversRoi=true`。
- 屏蔽 ROI 显隐：
  - 点击 `完成` 后，屏蔽 ROI 自动隐藏。
  - 仅进入屏蔽区域编辑状态时显示屏蔽 ROI，多边形仍可拖动和拖动角点。
- 验证：
  - 新增 `tests/color_recognition_mask_smoke.cpp`，覆盖“屏蔽 ROI 完全包含检测 ROI 时不检测并返回 `masked_roi_empty`”。
  - 已执行该 smoke 测试，通过。
  - 已执行 `/home/tt/Qt/5.15.2/gcc_64/bin/qmake qt_ui_test.pro && make -j8`，编译链接通过。 

### 新需求
- 颜色识别主颜色占比与光照稳定性优化计划

### 2026-07-01 14:20:44 CST - 测试运行控件新规范落地

- 按 `docs/FID/Function_Docs.md` 的 `测试运行统一规范` 调整颜色识别底部动作按钮。
- 编辑态底部按钮统一为：`基准图测试`、`测试运行`、`完成`。
- 点击 `测试运行` 后直接进入连续运行测试态：
  - 连续运行只使用 `CameraFrameProvider::currentFrame()` 的实时最新帧。
  - 按钮文字切换为 `停止运行`，并使用橙色高亮。
  - 点击 `停止运行` 后停止定时测试，按钮切换为 `连续运行`，视图保留最后一帧。
- 测试态中 `完成` 按钮切换为 `运行一次`：
  - 若正在连续运行，先停止连续运行。
  - 单次运行只取实时最新帧，不使用基准图。
- 测试态新增 `退出测试`：
  - 停止连续运行。
  - 恢复编辑态按钮。
  - 视图回到编辑预览状态。
- 新增 `基准图测试`：
  - 只使用 `ReferenceImageProvider::referenceFrame()`。
  - 基准图为空时返回 `no_reference_image`。
  - 不再回退到实时相机帧。
  - 成功结果继续生成 `referencePreviewSnapshot`。
- 验证：
  - 已执行 `/home/tt/Qt/5.15.2/gcc_64/bin/qmake qt_ui_test.pro && make -j8`，编译链接通过。

### 2026-07-01 位置修正占位链路补齐

- 按 `docs/FID/Function_Docs.md` 的位置修正统一规范补齐颜色识别链路。
- `ColorRecognitionDialog`：
  - 在 `检测区域` 卡片中新增 `独立位置修正使能 ⓘ` 开关。
  - 开关开启时显示 `位置修正` 来源下拉框。
  - 默认来源为 `1 基准图.位置修正信息`。
  - 保存字段为 `params.enablePositionCorrection` 和 `params.positionCorrectionSource`。
  - 旧配置缺少字段时默认关闭。
- `ColorRecognitionAdapter`：
  - 解析并透传 `enablePositionCorrection` 和 `positionCorrectionSource`。
- `ColorRecognitionHalconRunner`：
  - `ColorRecognitionHalconConfig` 增加 `enablePositionCorrection` 和 `positionCorrectionSource`。
  - 正常、错误、完全屏蔽等 payload 均输出位置修正状态。
  - 当前仅为占位链路，未实际进行 ROI 坐标补偿。
  - payload 固定输出 `positionCorrectionApplied=false`、`positionCorrectionReason=not implemented`。
- 验证：
  - 扩展 `tests/color_recognition_mask_smoke.cpp`，覆盖位置修正开启时 payload 输出。
  - 已执行颜色识别 smoke，验证通过。

### 2026-07-01 测试按钮族样式 C 方案分层权重

- 按 `docs/superpowers/specs/2026-07-01-color-test-buttons-style-design.md` 落地测试按钮族视觉样式，**仅改样式，不动任何点击/状态逻辑**。
- 三档视觉层级：
  - `完成 / 运行一次`（`actionRole=testPrimary`）：深色实心 `#111827` 白字，hover 更黑 `#000`，点击/flash 反色，disabled 灰化。
  - `测试运行 / 基准图测试`（`actionRole=testAction`）：白底灰边 `#9ca3af`，hover 浅灰底 `#f9fafb`+深边，点击/flash 反色，**连续运行态 `[running="true"]` 橙色实心 `#ff7a00` 白字**（此前 QSS 无 running 选择器，连续运行态与普通态无视觉区别），disabled 灰化。
  - `退出测试`（`#exitTestButton`，靠 objectName 定向覆盖 testAction 默认态）：透明描边浅灰字，hover 浅红底 `#fef2f2`+红字红边 `#dc2626`，点击/flash 红底白字，disabled 灰化。
- 补齐了原 QSS 缺失的 `:hover`、`[running="true"]`、`:disabled` 三类规则。
- 本次更改：
  - `src/ColorRecognitionDialog.cpp`：替换 `setupUiState()` 内联 QSS 块为完整规则集。该侧按钮属性已齐全（`testRunButton` 有 `actionRole=testAction`+`running`，`finishButton` 有 `testPrimary`，`m_exitTestButton` 已有 `setObjectName("exitTestButton")`），无需补 objectName。
  - 不改动：`installActionButtonFlash`、`running` 属性设置点、`refreshButtonStyle`、`applyBottomActionButtonMetrics`、所有信号槽连接。
- Qt QSS 约束：不支持 `box-shadow`/`animation`/`transition`/伪元素，故浏览器原型的 hover 阴影、running 呼吸光晕、过渡动画均降级为纯色/边框变化，三档层级靠底色与边框色区分。
- 验证：
  - 已执行 `/home/tt/Qt/5.15.2/gcc_64/bin/qmake qt_ui_test.pro && make -j$(nproc)`，强制重编 `ColorRecognitionDialog.cpp` + 链接通过，`-Wall -Wextra` 无相关警告。
  - 待手动 GUI 确认：完成/测试运行/基准图测试/退出测试四按钮的默认、hover、点击 flash、连续运行橙色态、disabled 灰化是否符合预期，且不影响基础/全部分段、ROI 绘制等回归。
- 剩余事项：
  - 真实 GUI 手动确认上述视觉与回归项。
