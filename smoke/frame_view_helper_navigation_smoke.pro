QT += core gui widgets
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = frame_view_helper_navigation_smoke

BUILD_ROOT = $$_PRO_FILE_PWD_/../build/smoke/frame_view_helper_navigation
DESTDIR = $$BUILD_ROOT/bin
OBJECTS_DIR = $$BUILD_ROOT/obj
MOC_DIR = $$BUILD_ROOT/moc
RCC_DIR = $$BUILD_ROOT/rcc
UI_DIR = $$BUILD_ROOT/ui
system(mkdir -p $$DESTDIR $$OBJECTS_DIR $$MOC_DIR $$RCC_DIR $$UI_DIR)

INCLUDEPATH += ../src
SOURCES += \
    frame_view_helper_navigation_smoke.cpp \
    ../src/frame/FrameViewHelper.cpp
HEADERS += \
    ../src/frame/FrameViewHelper.h \
    ../src/toolcore/ToolOverlay.h
