QT += core gui
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = color_comparison_smoke

BUILD_ROOT = $$_PRO_FILE_PWD_/../build/smoke/color_comparison
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
include(../qmake/halcon_20_11.pri)

SOURCES += \
    color_comparison_smoke.cpp \
    ../src/algorithms/halcon/HalconRuntimePaths.cpp \
    ../src/algorithms/recognition/ColorRecognitionHalconRunner.cpp \
    ../src/algorithms/recognition/ColorComparisonHalconRunner.cpp

LIBS += -L$$OPENCV_ROOT/lib
LIBS += -Wl,-rpath,$$OPENCV_ROOT/lib
LIBS += -lopencv_core -lopencv_imgproc -lopencv_imgcodecs -ldl
