QT += core gui
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = color_comparison_smoke

INCLUDEPATH += ../src
OPENCV_ROOT = /home/tt/.local/opencv-4.8.0
INCLUDEPATH += $$OPENCV_ROOT/include/opencv4
HALCON_ROOT = $$(HALCONROOT)
!exists($$HALCON_ROOT/include/HalconC.h) {
    HALCON_ROOT = /home/tt/tfk/WorkerSpace/Software/HALCON-24.11.1.0-Progress-Steady
}
INCLUDEPATH += $$HALCON_ROOT/include

SOURCES += \
    color_comparison_smoke.cpp \
    ../src/algorithms/halcon/HalconRuntimePaths.cpp \
    ../src/algorithms/recognition/ColorRecognitionHalconRunner.cpp \
    ../src/algorithms/recognition/ColorComparisonHalconRunner.cpp

LIBS += -L$$OPENCV_ROOT/lib
LIBS += -Wl,-rpath,$$OPENCV_ROOT/lib
LIBS += -lopencv_core -lopencv_imgproc -lopencv_imgcodecs -ldl
