QT += core gui widgets concurrent
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = color_recognition_gmm_b2_lifecycle_smoke

BUILD_ROOT = $$_PRO_FILE_PWD_/../build/smoke/color_recognition_gmm_b2_lifecycle
DESTDIR = $$BUILD_ROOT/bin
OBJECTS_DIR = $$BUILD_ROOT/obj
MOC_DIR = $$BUILD_ROOT/moc
RCC_DIR = $$BUILD_ROOT/rcc
UI_DIR = $$BUILD_ROOT/ui
system(mkdir -p $$DESTDIR $$OBJECTS_DIR $$MOC_DIR $$RCC_DIR $$UI_DIR)

INCLUDEPATH += ../src
include(../qmake/opencv.pri)
include(../qmake/halcon_20_11.pri)

SOURCES += \
    ../tests/color_recognition_gmm_b2_lifecycle_smoke.cpp \
    ../src/ColorTemplateDialog.cpp \
    ../src/frame/CameraFrameProvider.cpp \
    ../src/frame/FrameInputMetadata.cpp \
    ../src/frame/FramePixelProbe.cpp \
    ../src/frame/FrameViewHelper.cpp \
    ../src/frame/MatImageConverter.cpp \
    ../src/frame/ReferenceImageProvider.cpp \
    ../src/toolcore/PositionCorrection.cpp \
    ../src/tooladapters/ColorRecognitionAdapter.cpp \
    ../src/algorithms/halcon/HalconRuntimePaths.cpp \
    ../src/algorithms/recognition/ColorRecognitionGmmHalconBackend.cpp \
    ../src/algorithms/recognition/ColorRecognitionHalconRunner.cpp \
    ../src/algorithms/location/PositionCorrectionHalconTransform.cpp \
    ../src/toolcore/PositionCorrectionTransform.cpp

HEADERS += \
    ../src/ColorTemplateDialog.h \
    ../src/frame/CameraFrameProvider.h \
    ../src/frame/FramePixelProbe.h \
    ../src/frame/FrameViewHelper.h \
    ../src/frame/ReferenceImageProvider.h

LIBS += -ldl
