QT += core gui
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = template_location_smoke

isEmpty(BUILD_ROOT) {
    BUILD_ROOT = $$_PRO_FILE_PWD_/../build/smoke/template_location
}
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

SOURCES += \
    template_location_smoke.cpp \
    ../src/algorithms/halcon/HalconRuntimePaths.cpp \
    ../src/algorithms/location/TemplateLocationHalconRunner.cpp \
    ../src/algorithms/presence/PatternPresenceHalconApi.cpp \
    ../src/algorithms/presence/PatternPresenceAutoModelDomain.cpp \
    ../src/algorithms/presence/PatternPresenceHalconRunner.cpp \
    ../src/tooladapters/TemplateLocationAdapter.cpp

HEADERS += \
    ../src/algorithms/location/TemplateLocationHalconRunner.h \
    ../src/algorithms/presence/PatternPresenceHalconApi.h \
    ../src/algorithms/presence/PatternPresenceAutoModelDomain.h \
    ../src/algorithms/presence/PatternPresenceHalconRunner.h \
    ../src/tooladapters/TemplateLocationAdapter.h \
    ../src/toolcore/ToolAdapter.h \
    ../src/toolcore/ToolConfig.h \
    ../src/toolcore/ToolOverlay.h \
    ../src/toolcore/ToolRequest.h \
    ../src/toolcore/ToolResult.h \
    ../src/toolcore/ToolTypes.h

LIBS += -L$$OPENCV_ROOT/lib -Wl,-rpath,$$OPENCV_ROOT/lib
LIBS += -L$$HALCON_ROOT/lib/x64-linux -Wl,-rpath,$$HALCON_ROOT/lib/x64-linux
LIBS += -lopencv_core -lopencv_imgproc -lopencv_imgcodecs -lhalconcpp -lhalcon -ldl
