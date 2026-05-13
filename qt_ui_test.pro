QT += widgets
QT += multimedia multimediawidgets
CONFIG += c++17
TEMPLATE = app
TARGET = qt_ui_test

INCLUDEPATH += src
INCLUDEPATH += /usr/include/opencv4
INCLUDEPATH += /home/hjl-ubuntu/MVTec/HALCON-24.11-Progress-Steady/include

SOURCES += \
    src/main.cpp \
    src/LoginWindow.cpp \
    src/MainWindow.cpp \
    src/PlanDialogUtils.cpp \
    src/WindowUtils.cpp \
    src/CameraParamsDialog.cpp \
    src/ReferenceImageDialog.cpp \
    src/ToolLibraryDialog.cpp \
    src/CharacterRecognitionDialog.cpp \
    src/PatternPresenceDialog.cpp \
    src/BlobPresenceDialog.cpp \
    src/CirclePresenceDialog.cpp \
    src/ToolsDialog.cpp \
    src/OutputDialog.cpp \
    src/frame/CameraFrameProvider.cpp \
    src/frame/FrameViewHelper.cpp \
    src/frame/ReferenceImageProvider.cpp \
    src/toolcore/ToolEngine.cpp \
    src/tooladapters/OcrAdapter.cpp \
    src/tooladapters/PatternPresenceAdapter.cpp \
    src/tooladapters/BlobPresenceAdapter.cpp \
    src/tooladapters/CirclePresenceAdapter.cpp \
    src/algorithms/ocr/OcrHalconRunner.cpp \
    src/algorithms/presence/PatternPresenceHalconRunner.cpp \
    src/algorithms/presence/BlobPresenceHalconRunner.cpp \
    src/algorithms/presence/CirclePresenceHalconRunner.cpp

HEADERS += \
    src/LoginWindow.h \
    src/MainWindow.h \
    src/PlanDialogUtils.h \
    src/WindowUtils.h \
    src/CameraParamsDialog.h \
    src/ReferenceImageDialog.h \
    src/ToolLibraryDialog.h \
    src/CharacterRecognitionDialog.h \
    src/PatternPresenceDialog.h \
    src/BlobPresenceDialog.h \
    src/CirclePresenceDialog.h \
    src/ToolsDialog.h \
    src/OutputDialog.h \
    src/frame/CameraFrameProvider.h \
    src/frame/FrameViewHelper.h \
    src/frame/ReferenceImageProvider.h \
    src/toolcore/ToolTypes.h \
    src/toolcore/ToolConfig.h \
    src/toolcore/ToolRequest.h \
    src/toolcore/ToolResult.h \
    src/toolcore/ToolOverlay.h \
    src/toolcore/ToolAdapter.h \
    src/toolcore/ToolEngine.h \
    src/tooladapters/OcrAdapter.h \
    src/tooladapters/PatternPresenceAdapter.h \
    src/tooladapters/BlobPresenceAdapter.h \
    src/tooladapters/CirclePresenceAdapter.h \
    src/algorithms/ocr/OcrHalconRunner.h \
    src/algorithms/presence/PatternPresenceHalconRunner.h \
    src/algorithms/presence/BlobPresenceHalconRunner.h \
    src/algorithms/presence/CirclePresenceHalconRunner.h

FORMS += \
    ui/LoginWindow.ui \
    ui/MainWindow.ui \
    ui/CameraParamsDialog.ui \
    ui/ReferenceImageDialog.ui \
    ui/ToolLibraryDialog.ui \
    ui/CharacterRecognitionDialog.ui \
    ui/PatternPresenceDialog.ui \
    ui/BlobPresenceDialog.ui \
    ui/CirclePresenceDialog.ui \
    ui/ToolsDialog.ui \
    ui/OutputDialog.ui

RESOURCES += \
    resources/resources.qrc

# OpenCV库链接（移植自旧项目 qtt5_project_bak_327_10nrs_260328he/qtt5.pro）
LIBS += -lopencv_core -lopencv_imgproc -lopencv_highgui -lopencv_videoio -lopencv_imgcodecs
LIBS += -ldl
