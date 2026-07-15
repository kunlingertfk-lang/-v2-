QT += core gui widgets concurrent
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = color_comparison_dialog_integration_smoke

BUILD_ROOT = $$_PRO_FILE_PWD_/../build/smoke/color_comparison_dialog_integration
DESTDIR = $$BUILD_ROOT/bin
OBJECTS_DIR = $$BUILD_ROOT/obj
MOC_DIR = $$BUILD_ROOT/moc
RCC_DIR = $$BUILD_ROOT/rcc
UI_DIR = $$BUILD_ROOT/ui
system(mkdir -p $$DESTDIR $$OBJECTS_DIR $$MOC_DIR $$RCC_DIR $$UI_DIR)

INCLUDEPATH += ../src
OPENCV_ROOT = $$(OPENCV_ROOT)
isEmpty(OPENCV_ROOT): OPENCV_ROOT = $$(HOME)/.local/opencv-4.8.0
INCLUDEPATH += $$OPENCV_ROOT/include/opencv4

HALCON_ROOT = $$(HALCONROOT)
isEmpty(HALCON_ROOT): HALCON_ROOT = /opt/halcon
INCLUDEPATH += $$HALCON_ROOT/include

SOURCES += \
    color_comparison_dialog_integration_smoke.cpp \
    ../src/ColorComparisonDialog.cpp \
    ../src/ColorComparisonFeatureView.cpp \
    ../src/frame/CameraFrameProvider.cpp \
    ../src/frame/FrameInputMetadata.cpp \
    ../src/frame/FrameViewHelper.cpp \
    ../src/frame/MatImageConverter.cpp \
    ../src/frame/ReferenceImageProvider.cpp \
    ../src/tooladapters/ColorComparisonAdapter.cpp \
    ../src/algorithms/halcon/HalconRuntimePaths.cpp \
    ../src/algorithms/recognition/ColorComparisonHalconRunner.cpp \
    ../src/algorithms/recognition/ColorComparisonModel.cpp

HEADERS += \
    ../src/ColorComparisonDialog.h \
    ../src/ColorComparisonFeatureView.h \
    ../src/frame/CameraFrameProvider.h \
    ../src/frame/FrameViewHelper.h \
    ../src/frame/ReferenceImageProvider.h

LIBS += -L$$OPENCV_ROOT/lib -Wl,-rpath,$$OPENCV_ROOT/lib
LIBS += -lopencv_core -lopencv_imgproc -lopencv_videoio -lopencv_imgcodecs -ldl
