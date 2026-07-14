QT += core
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = registered_classification_adapter_smoke

BUILD_ROOT = $$_PRO_FILE_PWD_/../build/smoke/registered_classification_adapter
DESTDIR = $$BUILD_ROOT/bin
OBJECTS_DIR = $$BUILD_ROOT/obj
MOC_DIR = $$BUILD_ROOT/moc
RCC_DIR = $$BUILD_ROOT/rcc
UI_DIR = $$BUILD_ROOT/ui
system(mkdir -p $$DESTDIR $$OBJECTS_DIR $$MOC_DIR $$RCC_DIR $$UI_DIR)

INCLUDEPATH += ../src
OPENCV_ROOT = /home/tt/.local/opencv-4.8.0
INCLUDEPATH += $$OPENCV_ROOT/include/opencv4
include(../qmake/halcon_20_11.pri)

LIBS += -L$$OPENCV_ROOT/lib
LIBS += -Wl,-rpath,$$OPENCV_ROOT/lib
LIBS += -lopencv_core -lopencv_imgproc -ldl

SOURCES += \
    registered_classification_adapter_smoke.cpp \
    ../src/tooladapters/RegisteredClassificationAdapter.cpp \
    ../src/toolcore/PositionCorrection.cpp \
    ../src/algorithms/recognition/RegisteredClassificationFeatureSpace.cpp \
    ../src/algorithms/recognition/RegisteredClassificationKnnRuntime.cpp \
    ../src/algorithms/recognition/RegisteredClassificationModelPackage.cpp \
    ../src/algorithms/recognition/RegisteredClassificationTrainingSession.cpp \
    ../src/algorithms/recognition/RegisteredClassificationFeatureExtractor.cpp \
    ../src/algorithms/recognition/RegisteredClassificationTrainingRunner.cpp \
    ../src/algorithms/recognition/RegisteredClassificationHalconRunner.cpp \
    ../src/algorithms/halcon/HalconRuntimePaths.cpp

HEADERS += \
    ../src/tooladapters/RegisteredClassificationAdapter.h \
    ../src/algorithms/recognition/RegisteredClassificationFeatureSpace.h \
    ../src/algorithms/recognition/RegisteredClassificationKnnRuntime.h \
    ../src/algorithms/recognition/RegisteredClassificationModelPackage.h \
    ../src/algorithms/recognition/RegisteredClassificationTrainingSession.h \
    ../src/algorithms/recognition/RegisteredClassificationFeatureExtractor.h \
    ../src/algorithms/recognition/RegisteredClassificationTrainingRunner.h \
    ../src/algorithms/recognition/RegisteredClassificationHalconRunner.h \
    ../src/algorithms/halcon/HalconRuntimePaths.h \
    ../src/toolcore/ToolAdapter.h \
    ../src/toolcore/ToolConfig.h \
    ../src/toolcore/ToolOverlay.h \
    ../src/toolcore/PositionCorrection.h \
    ../src/toolcore/ToolRequest.h \
    ../src/toolcore/ToolResult.h \
    ../src/toolcore/ToolTypes.h
