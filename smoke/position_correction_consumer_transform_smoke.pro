QT += core gui
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = position_correction_consumer_transform_smoke

isEmpty(BUILD_ROOT): BUILD_ROOT = $$_PRO_FILE_PWD_/../build/smoke/position_correction_consumer_transform
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

SOURCES += \
    position_correction_consumer_transform_smoke.cpp \
    ../src/toolcore/PositionCorrection.cpp \
    ../src/toolcore/PositionCorrectionConsumer.cpp \
    ../src/toolcore/PositionCorrectionTransform.cpp

LIBS += -L$$OPENCV_ROOT/lib -Wl,-rpath,$$OPENCV_ROOT/lib
LIBS += -lopencv_core
