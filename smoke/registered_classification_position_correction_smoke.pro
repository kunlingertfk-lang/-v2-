QT += core gui
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = registered_classification_position_correction_smoke

BUILD_ROOT = $$_PRO_FILE_PWD_/../build/smoke/registered_classification_position_correction
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
INCLUDEPATH += $$HALCON_ROOT/include/halconcpp

LIBS += -L$$OPENCV_ROOT/lib
LIBS += -Wl,-rpath,$$OPENCV_ROOT/lib
LIBS += -lopencv_core -lopencv_imgproc -ldl
LIBS += -L$$HALCON_ROOT/lib/x64-linux
LIBS += -Wl,-rpath,$$HALCON_ROOT/lib/x64-linux
LIBS += -lhalconcpp -lhalcon

SOURCES += \
    registered_classification_position_correction_smoke.cpp \
    ../src/tooladapters/RegisteredClassificationAdapter.cpp \
    ../src/toolcore/PositionCorrection.cpp \
    ../src/toolcore/PositionCorrectionConsumer.cpp \
    ../src/toolcore/PositionCorrectionTransform.cpp \
    ../src/toolcore/ToolEngine.cpp \
    ../src/algorithms/location/PositionCorrectionHalconRunner.cpp \
    ../src/algorithms/location/PositionCorrectionHalconTransform.cpp \
    ../src/algorithms/location/TemplateLocationHalconRunner.cpp \
    ../src/algorithms/recognition/RegisteredClassificationFeatureSpace.cpp \
    ../src/algorithms/recognition/RegisteredClassificationModelPackage.cpp \
    ../src/algorithms/recognition/RegisteredClassificationTrainingSession.cpp \
    ../src/algorithms/recognition/RegisteredClassificationFeatureExtractor.cpp \
    ../src/algorithms/recognition/RegisteredClassificationKnnRuntime.cpp \
    ../src/algorithms/recognition/RegisteredClassificationTrainingRunner.cpp \
    ../src/algorithms/recognition/RegisteredClassificationHalconRunner.cpp \
    ../src/algorithms/halcon/HalconRuntimePaths.cpp

HEADERS += \
    ../src/tooladapters/RegisteredClassificationAdapter.h \
    ../src/toolcore/PositionCorrection.h \
    ../src/toolcore/PositionCorrectionConsumer.h \
    ../src/toolcore/PositionCorrectionTransform.h \
    ../src/toolcore/ToolAdapter.h \
    ../src/toolcore/ToolConfig.h \
    ../src/toolcore/ToolEngine.h \
    ../src/toolcore/ToolOverlay.h \
    ../src/toolcore/ToolRequest.h \
    ../src/toolcore/ToolResult.h \
    ../src/toolcore/ToolTypes.h \
    ../src/algorithms/location/PositionCorrectionHalconRunner.h \
    ../src/algorithms/location/PositionCorrectionHalconTransform.h \
    ../src/algorithms/location/TemplateLocationHalconRunner.h \
    ../src/algorithms/recognition/RegisteredClassificationFeatureSpace.h \
    ../src/algorithms/recognition/RegisteredClassificationModelPackage.h \
    ../src/algorithms/recognition/RegisteredClassificationTrainingSession.h \
    ../src/algorithms/recognition/RegisteredClassificationFeatureExtractor.h \
    ../src/algorithms/recognition/RegisteredClassificationKnnRuntime.h \
    ../src/algorithms/recognition/RegisteredClassificationTrainingRunner.h \
    ../src/algorithms/recognition/RegisteredClassificationHalconRunner.h \
    ../src/algorithms/halcon/HalconRuntimePaths.h
