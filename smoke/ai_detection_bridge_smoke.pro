QT += core gui
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = ai_detection_bridge_smoke

INCLUDEPATH += ../src
INCLUDEPATH += /usr/include/opencv4

DESTDIR = /tmp/v2_ai_bridge_smoke
OBJECTS_DIR = /tmp/v2_ai_bridge_smoke/obj
MOC_DIR = /tmp/v2_ai_bridge_smoke/moc
RCC_DIR = /tmp/v2_ai_bridge_smoke/rcc
UI_DIR = /tmp/v2_ai_bridge_smoke/ui

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
