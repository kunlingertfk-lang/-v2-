QT += core gui widgets
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = frame_view_helper_pixel_probe_smoke

BUILD_ROOT = $$_PRO_FILE_PWD_/../build/smoke/frame_view_helper_pixel_probe
DESTDIR = $$BUILD_ROOT/bin
OBJECTS_DIR = $$BUILD_ROOT/obj
MOC_DIR = $$BUILD_ROOT/moc
RCC_DIR = $$BUILD_ROOT/rcc
UI_DIR = $$BUILD_ROOT/ui
system(mkdir -p $$DESTDIR $$OBJECTS_DIR $$MOC_DIR $$RCC_DIR $$UI_DIR)

INCLUDEPATH += ../src
SOURCES += \
    frame_view_helper_pixel_probe_smoke.cpp \
    ../src/frame/FramePixelProbe.cpp \
    ../src/frame/FrameViewHelper.cpp
HEADERS += \
    ../src/frame/FramePixelProbe.h \
    ../src/frame/FrameViewHelper.h \
    ../src/frame/RoiGeometry.h \
    ../src/frame/RoiEditorController.h \
    ../src/toolcore/ToolOverlay.h
