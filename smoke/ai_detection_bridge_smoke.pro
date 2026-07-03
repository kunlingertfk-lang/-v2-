QT += core gui
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = ai_detection_bridge_smoke

INCLUDEPATH += ../src
INCLUDEPATH += /usr/include/opencv4

BUILD_ROOT = $$_PRO_FILE_PWD_/../build/smoke/ai_detection_bridge
DESTDIR = $$BUILD_ROOT/bin
OBJECTS_DIR = $$BUILD_ROOT/obj
MOC_DIR = $$BUILD_ROOT/moc
RCC_DIR = $$BUILD_ROOT/rcc
UI_DIR = $$BUILD_ROOT/ui
system(mkdir -p $$DESTDIR $$OBJECTS_DIR $$MOC_DIR $$RCC_DIR $$UI_DIR)

SOURCES += \
    ai_detection_bridge_smoke.cpp \
    ../src/algorithms/ai/AiDetectionRunner.cpp \
    ../src/tooladapters/AiDetectionAdapter.cpp

HEADERS += \
    ../src/algorithms/ai/AiDetectionRunner.h \
    ../src/tooladapters/AiDetectionAdapter.h \
    ../src/toolcore/ToolAdapter.h \
    ../src/toolcore/ToolConfig.h \
    ../src/toolcore/ToolOverlay.h \
    ../src/toolcore/ToolRequest.h \
    ../src/toolcore/ToolResult.h \
    ../src/toolcore/ToolTypes.h

LIBS += -lopencv_core -lopencv_imgcodecs
