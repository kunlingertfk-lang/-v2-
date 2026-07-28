QT += core
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = position_correction_engine_context_smoke

isEmpty(BUILD_ROOT): BUILD_ROOT = $$_PRO_FILE_PWD_/../build/smoke/position_correction_engine_context
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
LIBS += -L$$OPENCV_ROOT/lib -Wl,-rpath,$$OPENCV_ROOT/lib -lopencv_core -lopencv_imgproc

HALCONROOT = $$(HALCONROOT)
isEmpty(HALCONROOT) {
    HALCONROOT = /opt/halcon
}
HALCON_LIBDIR = $$HALCONROOT/lib/x64-linux
INCLUDEPATH += $$HALCONROOT/include $$HALCONROOT/include/halconcpp
LIBS += -L$$HALCON_LIBDIR -Wl,-rpath,$$HALCON_LIBDIR -lhalconcpp -lhalcon -ldl

SOURCES += \
    position_correction_engine_context_smoke.cpp \
    ../src/algorithms/halcon/HalconRuntimePaths.cpp \
    ../src/algorithms/location/PositionCorrectionHalconRunner.cpp \
    ../src/algorithms/location/TemplateLocationHalconRunner.cpp \
    ../src/toolcore/PositionCorrection.cpp \
    ../src/toolcore/ToolEngine.cpp

HEADERS += \
    ../src/algorithms/halcon/HalconRuntimePaths.h \
    ../src/algorithms/location/PositionCorrectionHalconRunner.h \
    ../src/algorithms/location/TemplateLocationHalconRunner.h \
    ../src/toolcore/PositionCorrection.h \
    ../src/toolcore/ToolAdapter.h \
    ../src/toolcore/ToolConfig.h \
    ../src/toolcore/ToolEngine.h \
    ../src/toolcore/ToolOverlay.h \
    ../src/toolcore/ToolRequest.h \
    ../src/toolcore/ToolResult.h \
    ../src/toolcore/ToolTypes.h
