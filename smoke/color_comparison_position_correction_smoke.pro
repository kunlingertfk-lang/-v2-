QT += core gui
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = color_comparison_position_correction_smoke

isEmpty(BUILD_ROOT): BUILD_ROOT = $$_PRO_FILE_PWD_/../build/smoke/color_comparison_position_correction
DESTDIR = $$BUILD_ROOT/bin
OBJECTS_DIR = $$BUILD_ROOT/obj
MOC_DIR = $$BUILD_ROOT/moc
RCC_DIR = $$BUILD_ROOT/rcc
UI_DIR = $$BUILD_ROOT/ui
system(mkdir -p $$DESTDIR $$OBJECTS_DIR $$MOC_DIR $$RCC_DIR $$UI_DIR)

INCLUDEPATH += ../src
OPENCV_ROOT = $$(OPENCV_ROOT)
isEmpty(OPENCV_ROOT): OPENCV_ROOT = $$(HOME)/.local/opencv-4.8.0
INCLUDEPATH += $$OPENCV_ROOT/include/opencv4
HALCON_ROOT = $$(HALCONROOT)
isEmpty(HALCON_ROOT): HALCON_ROOT = /opt/halcon
INCLUDEPATH += $$HALCON_ROOT/include $$HALCON_ROOT/include/halconcpp

SOURCES += \
    color_comparison_position_correction_smoke.cpp \
    ../src/tooladapters/ColorComparisonAdapter.cpp \
    ../src/toolcore/PositionCorrection.cpp \
    ../src/toolcore/PositionCorrectionConsumer.cpp \
    ../src/toolcore/PositionCorrectionTransform.cpp \
    ../src/toolcore/ToolEngine.cpp \
    ../src/algorithms/halcon/HalconRuntimePaths.cpp \
    ../src/algorithms/location/PositionCorrectionHalconRunner.cpp \
    ../src/algorithms/location/PositionCorrectionHalconTransform.cpp \
    ../src/algorithms/location/TemplateLocationHalconRunner.cpp \
    ../src/algorithms/recognition/ColorComparisonHalconRunner.cpp \
    ../src/algorithms/recognition/ColorComparisonModel.cpp

LIBS += -L$$OPENCV_ROOT/lib -Wl,-rpath,$$OPENCV_ROOT/lib
LIBS += -lopencv_core -lopencv_imgproc -ldl
LIBS += -L$$HALCON_ROOT/lib/x64-linux -Wl,-rpath,$$HALCON_ROOT/lib/x64-linux
LIBS += -lhalconcpp -lhalcon
