QT += core
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = registered_classification_adapter_smoke

INCLUDEPATH += ../src
OPENCV_ROOT = /home/tt/.local/opencv-4.8.0
INCLUDEPATH += $$OPENCV_ROOT/include/opencv4
HALCON_ROOT = $$(HALCONROOT)
!exists($$HALCON_ROOT/include/HalconC.h) {
    HALCON_ROOT = /home/tt/tfk/WorkerSpace/Software/HALCON-24.11.1.0-Progress-Steady
}
INCLUDEPATH += $$HALCON_ROOT/include

LIBS += -L$$OPENCV_ROOT/lib
LIBS += -Wl,-rpath,$$OPENCV_ROOT/lib
LIBS += -lopencv_core -lopencv_imgproc -ldl

SOURCES += \
    registered_classification_adapter_smoke.cpp \
    ../src/tooladapters/RegisteredClassificationAdapter.cpp \
    ../src/algorithms/recognition/RegisteredClassificationHalconRunner.cpp \
    ../src/algorithms/halcon/HalconRuntimePaths.cpp

HEADERS += \
    ../src/tooladapters/RegisteredClassificationAdapter.h \
    ../src/algorithms/recognition/RegisteredClassificationHalconRunner.h \
    ../src/algorithms/halcon/HalconRuntimePaths.h \
    ../src/toolcore/ToolAdapter.h \
    ../src/toolcore/ToolConfig.h \
    ../src/toolcore/ToolOverlay.h \
    ../src/toolcore/ToolRequest.h \
    ../src/toolcore/ToolResult.h \
    ../src/toolcore/ToolTypes.h

