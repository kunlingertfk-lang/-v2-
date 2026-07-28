QT += core gui
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = reference_position_correction_private_model_smoke

isEmpty(BUILD_ROOT): BUILD_ROOT = $$_PRO_FILE_PWD_/../build/smoke/reference_position_correction_private_model
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
    reference_position_correction_private_model_smoke.cpp \
    ../src/algorithms/halcon/HalconRuntimePaths.cpp \
    ../src/algorithms/location/TemplateLocationHalconRunner.cpp

HEADERS += \
    ../src/algorithms/halcon/HalconRuntimePaths.h \
    ../src/algorithms/location/TemplateLocationHalconRunner.h \
    ../src/toolcore/ToolOverlay.h \
    ../src/toolcore/ToolTypes.h
