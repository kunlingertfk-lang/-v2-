QT += core gui widgets
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = position_correction_ui_smoke

BUILD_ROOT = $$_PRO_FILE_PWD_/../build/smoke/position_correction_ui
DESTDIR = $$BUILD_ROOT/bin
OBJECTS_DIR = $$BUILD_ROOT/obj
MOC_DIR = $$BUILD_ROOT/moc
RCC_DIR = $$BUILD_ROOT/rcc
UI_DIR = $$BUILD_ROOT/ui
system(mkdir -p $$DESTDIR $$OBJECTS_DIR $$MOC_DIR $$RCC_DIR $$UI_DIR)

INCLUDEPATH += ../src
OPENCV_ROOT = /home/tt/.local/opencv-4.8.0
INCLUDEPATH += $$OPENCV_ROOT/include/opencv4
LIBS += -L$$OPENCV_ROOT/lib -Wl,-rpath,$$OPENCV_ROOT/lib -lopencv_core -lopencv_imgproc

SOURCES += \
    position_correction_ui_smoke.cpp \
    ../src/PositionCorrectionDialog.cpp \
    ../src/frame/FrameViewHelper.cpp \
    ../src/frame/MatImageConverter.cpp \
    ../src/frame/ReferenceImageProvider.cpp \
    ../src/toolcore/PositionCorrection.cpp

HEADERS += \
    ../src/PositionCorrectionDialog.h \
    ../src/frame/FrameViewHelper.h \
    ../src/frame/MatImageConverter.h \
    ../src/frame/ReferenceImageProvider.h \
    ../src/toolcore/PositionCorrection.h \
    ../src/toolcore/ToolConfig.h \
    ../src/toolcore/ToolPreviewSnapshot.h \
    ../src/toolcore/ToolTypes.h

FORMS += ../ui/PositionCorrectionDialog.ui
