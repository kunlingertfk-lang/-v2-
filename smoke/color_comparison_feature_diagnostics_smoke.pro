QT += core gui
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = color_comparison_feature_diagnostics_smoke

BUILD_ROOT = $$_PRO_FILE_PWD_/../build/smoke/color_comparison_feature_diagnostics
DESTDIR = $$BUILD_ROOT/bin
OBJECTS_DIR = $$BUILD_ROOT/obj
MOC_DIR = $$BUILD_ROOT/moc
RCC_DIR = $$BUILD_ROOT/rcc
UI_DIR = $$BUILD_ROOT/ui
system(mkdir -p $$DESTDIR $$OBJECTS_DIR $$MOC_DIR $$RCC_DIR $$UI_DIR)

INCLUDEPATH += ../src
OPENCV_ROOT = $$(OPENCV_ROOT)
isEmpty(OPENCV_ROOT) {
    OPENCV_ROOT = $$(HOME)/.local/opencv-4.8.0
}
INCLUDEPATH += $$OPENCV_ROOT/include/opencv4
HALCON_ROOT = $$(HALCONROOT)
isEmpty(HALCON_ROOT): HALCON_ROOT = /opt/halcon
!exists($$HALCON_ROOT/include/HalconC.h) {
    error("HALCONROOT=$$HALCON_ROOT lacks include/HalconC.h")
}
INCLUDEPATH += $$HALCON_ROOT/include

SOURCES += \
    color_comparison_feature_diagnostics_smoke.cpp \
    ../src/algorithms/halcon/HalconRuntimePaths.cpp \
    ../src/algorithms/recognition/ColorComparisonModel.cpp \
    ../src/algorithms/recognition/ColorComparisonHalconRunner.cpp

HEADERS += \
    ../src/algorithms/halcon/HalconRuntimePaths.h \
    ../src/algorithms/recognition/ColorComparisonModel.h \
    ../src/algorithms/recognition/ColorComparisonHalconRunner.h \
    ../src/toolcore/ToolOverlay.h

LIBS += -L$$OPENCV_ROOT/lib -Wl,-rpath,$$OPENCV_ROOT/lib
LIBS += -lopencv_core -lopencv_imgproc -lopencv_imgcodecs -ldl
