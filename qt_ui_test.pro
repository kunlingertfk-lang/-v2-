QT += widgets
QT += concurrent
QT += multimedia multimediawidgets
CONFIG += c++17
TEMPLATE = app
TARGET = qt_ui_test

INCLUDEPATH += src
INCLUDEPATH += /usr/include/opencv4
HALCON_ROOT = $$(HALCONROOT)
!exists($$HALCON_ROOT/include/HalconC.h) {
    HALCON_ROOT = /home/superhe/桌面/som-halcon/repository/packages.mvtec.com/halcon/halcon-24.11-progress-steady/halcon-24.11.2.0-development_general-x64-linux
}
INCLUDEPATH += $$HALCON_ROOT/include


https://proxyinfo.net/api/v1/client/subscribe?token=04bbb5952682d8967d1b8baa8f60f6e6
先不要修改任何文件。

请你阅读当前这个项目，然后用适合新手的方式回答：

1. 这个项目是做什么的？
2. 它主要解决什么问题？
3. 项目里最重要的几个目录分别是干什么的？
4. 源码大概放在哪里？
5. 测试大概放在哪里？
6. 文档大概放在哪里？

回答时请尽量引用具体文件路径。
然后写进 项目分析.md 今后的一些分析和指令文档都存在一个文件夹下



SOURCES += \
    src/main.cpp \
    src/LoginWindow.cpp \
    src/MainWindow.cpp \
    src/SchemeStore.cpp \
    src/PlanDialogUtils.cpp \
    src/CameraParamsDialog.cpp \
    src/ReferenceImageDialog.cpp \
    src/ToolLibraryDialog.cpp \
    src/CharacterRecognitionDialog.cpp \
    src/ClassificationDialog.cpp \
    src/ObjectDetectionDialog.cpp \
    src/PatternPresenceDialog.cpp \
    src/BlobPresenceDialog.cpp \
    src/CirclePresenceDialog.cpp \
    src/EdgePresenceDialog.cpp \
    src/LinePresenceDialog.cpp \
    src/ContourPresenceDialog.cpp \
    src/ToolsDialog.cpp \
    src/OutputDialog.cpp \
    src/frame/CameraFrameProvider.cpp \
    src/frame/FrameViewHelper.cpp \
    src/frame/MatImageConverter.cpp \
    src/frame/ReferenceImageProvider.cpp \
    src/toolcore/ToolEngine.cpp \
    src/tooladapters/OcrAdapter.cpp \
    src/tooladapters/PatternPresenceAdapter.cpp \
    src/tooladapters/BlobPresenceAdapter.cpp \
    src/tooladapters/CirclePresenceAdapter.cpp \
    src/tooladapters/EdgePresenceAdapter.cpp \
    src/tooladapters/LinePresenceAdapter.cpp \
    src/tooladapters/ContourPresenceAdapter.cpp \
    src/tooladapters/AiDetectionAdapter.cpp \
    src/algorithms/halcon/HalconRuntimePaths.cpp \
    src/algorithms/ocr/OcrHalconRunner.cpp \
    src/algorithms/ai/AiDetectionRunner.cpp \
    src/algorithms/presence/PatternPresenceHalconApi.cpp \
    src/algorithms/presence/PatternPresenceAutoModelDomain.cpp \
    src/algorithms/presence/PatternPresenceHalconRunner.cpp \
    src/algorithms/presence/BlobPresenceHalconRunner.cpp \
    src/algorithms/presence/CirclePresenceHalconRunner.cpp \
    src/algorithms/presence/EdgePresenceHalconRunner.cpp \
    src/algorithms/presence/LinePresenceHalconRunner.cpp \
    src/algorithms/presence/ContourPresenceHalconRunner.cpp

HEADERS += \
    src/LoginWindow.h \
    src/MainWindow.h \
    src/SchemeStore.h \
    src/PlanDialogUtils.h \
    src/CameraParamsDialog.h \
    src/ReferenceImageDialog.h \
    src/ToolLibraryDialog.h \
    src/CharacterRecognitionDialog.h \
    src/ClassificationDialog.h \
    src/ObjectDetectionDialog.h \
    src/PatternPresenceDialog.h \
    src/BlobPresenceDialog.h \
    src/CirclePresenceDialog.h \
    src/EdgePresenceDialog.h \
    src/LinePresenceDialog.h \
    src/ContourPresenceDialog.h \
    src/ToolsDialog.h \
    src/OutputDialog.h \
    src/frame/CameraFrameProvider.h \
    src/frame/FrameViewHelper.h \
    src/frame/MatImageConverter.h \
    src/frame/ReferenceImageProvider.h \
    src/toolcore/ToolTypes.h \
    src/toolcore/ToolConfig.h \
    src/toolcore/ToolRequest.h \
    src/toolcore/ToolResult.h \
    src/toolcore/ToolOverlay.h \
    src/toolcore/ToolPreviewSnapshot.h \
    src/toolcore/ToolAdapter.h \
    src/toolcore/ToolEngine.h \
    src/tooladapters/OcrAdapter.h \
    src/tooladapters/PatternPresenceAdapter.h \
    src/tooladapters/BlobPresenceAdapter.h \
    src/tooladapters/CirclePresenceAdapter.h \
    src/tooladapters/EdgePresenceAdapter.h \
    src/tooladapters/LinePresenceAdapter.h \
    src/tooladapters/ContourPresenceAdapter.h \
    src/tooladapters/AiDetectionAdapter.h \
    src/algorithms/halcon/HalconRuntimePaths.h \
    src/algorithms/ocr/OcrHalconRunner.h \
    src/algorithms/ai/AiDetectionRunner.h \
    src/algorithms/presence/PatternPresenceHalconApi.h \
    src/algorithms/presence/PatternPresenceAutoModelDomain.h \
    src/algorithms/presence/PatternPresenceHalconRunner.h \
    src/algorithms/presence/BlobPresenceHalconRunner.h \
    src/algorithms/presence/CirclePresenceHalconRunner.h \
    src/algorithms/presence/EdgePresenceHalconRunner.h \
    src/algorithms/presence/LinePresenceHalconRunner.h \
    src/algorithms/presence/ContourPresenceHalconRunner.h

FORMS += \
    ui/LoginWindow.ui \
    ui/MainWindow.ui \
    ui/CameraParamsDialog.ui \
    ui/ReferenceImageDialog.ui \
    ui/ToolLibraryDialog.ui \
    ui/CharacterRecognitionDialog.ui \
    ui/ClassificationDialog.ui \
    ui/ObjectDetectionDialog.ui \
    ui/PatternPresenceDialog.ui \
    ui/BlobPresenceDialog.ui \
    ui/CirclePresenceDialog.ui \
    ui/EdgePresenceDialog.ui \
    ui/LinePresenceDialog.ui \
    ui/ContourPresenceDialog.ui \
    ui/ToolsDialog.ui \
    ui/OutputDialog.ui

RESOURCES += \
    resources/resources.qrc

# OpenCV库链接（移植自旧项目 qtt5_project_bak_327_10nrs_260328he/qtt5.pro）
LIBS += -lopencv_core -lopencv_imgproc -lopencv_highgui -lopencv_videoio -lopencv_imgcodecs
LIBS += -ldl
