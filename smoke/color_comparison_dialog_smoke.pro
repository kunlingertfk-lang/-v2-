QT += core gui widgets concurrent
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = color_comparison_dialog_smoke

BUILD_ROOT = $$_PRO_FILE_PWD_/../build/smoke/color_comparison_dialog
DESTDIR = $$BUILD_ROOT/bin
OBJECTS_DIR = $$BUILD_ROOT/obj
MOC_DIR = $$BUILD_ROOT/moc
RCC_DIR = $$BUILD_ROOT/rcc
UI_DIR = $$BUILD_ROOT/ui
system(mkdir -p $$DESTDIR $$OBJECTS_DIR $$MOC_DIR $$RCC_DIR $$UI_DIR)

INCLUDEPATH += ../src
OPENCV_ROOT = /home/tt/.local/opencv-4.8.0
INCLUDEPATH += $$OPENCV_ROOT/include/opencv4
HALCON_ROOT = $$(HALCONROOT)
!exists($$HALCON_ROOT/include/HalconC.h) {
    HALCON_ROOT = /home/tt/tfk/WorkerSpace/Software/HALCON-24.11.1.0-Progress-Steady
}
INCLUDEPATH += $$HALCON_ROOT/include

SOURCES += \
    color_comparison_dialog_smoke.cpp \
    ../src/ColorComparisonDialog.cpp \
    ../src/frame/CameraFrameProvider.cpp \
    ../src/frame/FrameInputMetadata.cpp \
    ../src/frame/FrameViewHelper.cpp \
    ../src/frame/MatImageConverter.cpp \
    ../src/frame/ReferenceImageProvider.cpp \
    ../src/algorithms/halcon/HalconRuntimePaths.cpp \
    ../src/algorithms/recognition/ColorComparisonModel.cpp \
    ../src/algorithms/recognition/ColorComparisonHalconRunner.cpp \
    ../src/tooladapters/ColorComparisonAdapter.cpp

HEADERS += \
    ../src/ColorComparisonDialog.h \
    ../src/PlanDialogUtils.h \
    ../src/frame/CameraFrameProvider.h \
    ../src/frame/FrameInputMetadata.h \
    ../src/frame/FrameViewHelper.h \
    ../src/frame/MatImageConverter.h \
    ../src/frame/ReferenceImageProvider.h \
    ../src/algorithms/halcon/HalconRuntimePaths.h \
    ../src/algorithms/recognition/ColorComparisonModel.h \
    ../src/algorithms/recognition/ColorComparisonHalconRunner.h \
    ../src/tooladapters/ColorComparisonAdapter.h \
    ../src/toolcore/ToolAdapter.h \
    ../src/toolcore/ToolConfig.h \
    ../src/toolcore/ToolOverlay.h \
    ../src/toolcore/ToolPreviewSnapshot.h \
    ../src/toolcore/ToolRequest.h \
    ../src/toolcore/ToolResult.h \
    ../src/toolcore/ToolTypes.h

LIBS += -L$$OPENCV_ROOT/lib
LIBS += -Wl,-rpath,$$OPENCV_ROOT/lib
LIBS += -lopencv_core -lopencv_imgproc -lopencv_videoio -ldl
