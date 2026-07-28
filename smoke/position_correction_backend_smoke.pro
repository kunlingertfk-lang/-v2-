QT += core gui
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = position_correction_backend_smoke

isEmpty(BUILD_ROOT): BUILD_ROOT = $$_PRO_FILE_PWD_/../build/smoke/position_correction_backend
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
    position_correction_backend_smoke.cpp \
    ../src/algorithms/location/PositionCorrectionHalconRunner.cpp \
    ../src/tooladapters/PositionCorrectionAdapter.cpp \
    ../src/toolcore/PositionCorrection.cpp

HEADERS += \
    ../src/algorithms/location/PositionCorrectionHalconRunner.h \
    ../src/tooladapters/PositionCorrectionAdapter.h \
    ../src/toolcore/PositionCorrection.h \
    ../src/toolcore/ToolAdapter.h \
    ../src/toolcore/ToolConfig.h \
    ../src/toolcore/ToolOverlay.h \
    ../src/toolcore/ToolRequest.h \
    ../src/toolcore/ToolResult.h \
    ../src/toolcore/ToolTypes.h

LIBS += -L$$HALCON_ROOT/lib/x64-linux -Wl,-rpath,$$HALCON_ROOT/lib/x64-linux
LIBS += -L$$OPENCV_ROOT/lib -Wl,-rpath,$$OPENCV_ROOT/lib
LIBS += -lhalconcpp -lhalcon -lopencv_core -ldl
