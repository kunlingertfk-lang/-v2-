QT += core gui
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = color_recognition_phase_a_smoke

BUILD_ROOT = $$_PRO_FILE_PWD_/../build/smoke/color_recognition_phase_a
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
    ../tests/color_recognition_mask_smoke.cpp \
    ../src/algorithms/halcon/HalconRuntimePaths.cpp \
    ../src/algorithms/recognition/ColorRecognitionHalconRunner.cpp \
    ../src/algorithms/location/PositionCorrectionHalconTransform.cpp \
    ../src/toolcore/PositionCorrectionTransform.cpp

LIBS += -ldl
