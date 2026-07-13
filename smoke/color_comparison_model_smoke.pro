QT += core
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = color_comparison_model_smoke

BUILD_ROOT = $$_PRO_FILE_PWD_/../build/smoke/color_comparison_model
DESTDIR = $$BUILD_ROOT/bin
OBJECTS_DIR = $$BUILD_ROOT/obj
MOC_DIR = $$BUILD_ROOT/moc
RCC_DIR = $$BUILD_ROOT/rcc
UI_DIR = $$BUILD_ROOT/ui
system(mkdir -p $$DESTDIR $$OBJECTS_DIR $$MOC_DIR $$RCC_DIR $$UI_DIR)

INCLUDEPATH += ../src
OPENCV_ROOT = /home/tt/.local/opencv-4.8.0
INCLUDEPATH += $$OPENCV_ROOT/include/opencv4

SOURCES += \
    color_comparison_model_smoke.cpp \
    ../src/algorithms/recognition/ColorComparisonModel.cpp

LIBS += -L$$OPENCV_ROOT/lib
LIBS += -Wl,-rpath,$$OPENCV_ROOT/lib
LIBS += -lopencv_core
