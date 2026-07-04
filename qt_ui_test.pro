QT += widgets
QT += concurrent
QT += multimedia multimediawidgets
CONFIG += c++17
TEMPLATE = app
TARGET = qt_ui_test

BUILD_ROOT = $$_PRO_FILE_PWD_/build/qt_ui_test
DESTDIR = $$BUILD_ROOT/bin
OBJECTS_DIR = $$BUILD_ROOT/obj
MOC_DIR = $$BUILD_ROOT/moc
RCC_DIR = $$BUILD_ROOT/rcc
UI_DIR = $$BUILD_ROOT/ui
system(mkdir -p $$DESTDIR $$OBJECTS_DIR $$MOC_DIR $$RCC_DIR $$UI_DIR)

INCLUDEPATH += src
OPENCV_ROOT = /home/tt/.local/opencv-4.8.0
INCLUDEPATH += $$OPENCV_ROOT/include/opencv4
HALCON_ROOT = $$(HALCONROOT)
!exists($$HALCON_ROOT/include/HalconC.h) {
    HALCON_ROOT = /home/tt/tfk/WorkerSpace/Software/HALCON-24.11.1.0-Progress-Steady
    message("Using bundled HALCON root: $$HALCON_ROOT")
}
INCLUDEPATH += $$HALCON_ROOT/include


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
    src/ColorRecognitionDialog.cpp \
    src/ColorComparisonDialog.cpp \
    src/RegisteredClassificationDialog.cpp \
    src/RegisteredClassificationTrainingDialog.cpp \
    src/RegisteredClassificationModelManagementDialog.cpp \
    src/ColorTemplateDialog.cpp \
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
    src/toolcore/PositionCorrection.cpp \
    src/toolcore/ToolEngine.cpp \
    src/tooladapters/OcrAdapter.cpp \
    src/tooladapters/ColorRecognitionAdapter.cpp \
    src/tooladapters/ColorComparisonAdapter.cpp \
    src/tooladapters/RegisteredClassificationAdapter.cpp \
    src/tooladapters/PatternPresenceAdapter.cpp \
    src/tooladapters/BlobPresenceAdapter.cpp \
    src/tooladapters/CirclePresenceAdapter.cpp \
    src/tooladapters/EdgePresenceAdapter.cpp \
    src/tooladapters/LinePresenceAdapter.cpp \
    src/tooladapters/ContourPresenceAdapter.cpp \
    src/tooladapters/AiDetectionAdapter.cpp \
    src/algorithms/halcon/HalconRuntimePaths.cpp \
    src/algorithms/ocr/OcrHalconRunner.cpp \
    src/algorithms/recognition/ColorRecognitionHalconRunner.cpp \
    src/algorithms/recognition/ColorComparisonHalconRunner.cpp \
    src/algorithms/recognition/RegisteredClassificationHalconRunner.cpp \
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
    src/ColorRecognitionDialog.h \
    src/ColorComparisonDialog.h \
    src/RegisteredClassificationDialog.h \
    src/RegisteredClassificationTrainingDialog.h \
    src/RegisteredClassificationModelManagementDialog.h \
    src/ColorTemplateDialog.h \
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
    src/toolcore/PositionCorrection.h \
    src/toolcore/ToolPreviewSnapshot.h \
    src/toolcore/ToolAdapter.h \
    src/toolcore/ToolEngine.h \
    src/tooladapters/OcrAdapter.h \
    src/tooladapters/ColorRecognitionAdapter.h \
    src/tooladapters/ColorComparisonAdapter.h \
    src/tooladapters/RegisteredClassificationAdapter.h \
    src/tooladapters/PatternPresenceAdapter.h \
    src/tooladapters/BlobPresenceAdapter.h \
    src/tooladapters/CirclePresenceAdapter.h \
    src/tooladapters/EdgePresenceAdapter.h \
    src/tooladapters/LinePresenceAdapter.h \
    src/tooladapters/ContourPresenceAdapter.h \
    src/tooladapters/AiDetectionAdapter.h \
    src/algorithms/halcon/HalconRuntimePaths.h \
    src/algorithms/ocr/OcrHalconRunner.h \
    src/algorithms/recognition/ColorRecognitionHalconRunner.h \
    src/algorithms/recognition/ColorComparisonHalconRunner.h \
    src/algorithms/recognition/RegisteredClassificationHalconRunner.h \
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
    ui/ColorRecognitionDialog.ui \
    ui/ColorComparisonDialog.ui \
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
LIBS += -L$$OPENCV_ROOT/lib
LIBS += -Wl,-rpath,$$OPENCV_ROOT/lib
LIBS += -lopencv_core -lopencv_imgproc -lopencv_highgui -lopencv_videoio -lopencv_imgcodecs
LIBS += -ldl
