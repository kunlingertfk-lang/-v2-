QT += core network widgets
CONFIG += console c++17
CONFIG += link_pkgconfig
CONFIG -= app_bundle
PKGCONFIG += opencv4
TEMPLATE = app
TARGET = calibration_core_smoke

isEmpty(BUILD_ROOT): BUILD_ROOT = $$_PRO_FILE_PWD_/../build/smoke/calibration_core
DESTDIR = $$BUILD_ROOT/bin
OBJECTS_DIR = $$BUILD_ROOT/obj
MOC_DIR = $$BUILD_ROOT/moc
RCC_DIR = $$BUILD_ROOT/rcc
UI_DIR = $$BUILD_ROOT/ui
system(mkdir -p $$DESTDIR $$OBJECTS_DIR $$MOC_DIR $$RCC_DIR $$UI_DIR)

INCLUDEPATH += ../src

HALCON_ROOT = /opt/halcon
HALCON_ARCH = x64-linux
INCLUDEPATH += $$HALCON_ROOT/include $$HALCON_ROOT/include/halconcpp
LIBS += -L$$HALCON_ROOT/lib/$$HALCON_ARCH \
        -Wl,-rpath,$$HALCON_ROOT/lib/$$HALCON_ARCH \
        -lhalconcpp -lhalcon -ldl

SOURCES += \
    calibration_core_smoke.cpp \
    ../src/calibration/CalibrationModel.cpp \
    ../src/calibration/CalibrationSolver.cpp \
    ../src/calibration/CalibrationFileLoader.cpp \
    ../src/calibration/CalibrationCommunicationProtocol.cpp \
    ../src/calibration/CalibrationMethodRegistry.cpp \
    ../src/calibration/NPointCalibrationConfigWidget.cpp \
    ../src/tooladapters/CalibrationTransformAdapter.cpp \
    ../src/algorithms/location/CalibrationTransformHalconRunner.cpp

HEADERS += \
    ../src/calibration/CalibrationModel.h \
    ../src/calibration/CalibrationSolver.h \
    ../src/calibration/CalibrationFileLoader.h \
    ../src/calibration/CalibrationCommunicationProtocol.h \
    ../src/calibration/CalibrationMethodRegistry.h \
    ../src/calibration/NPointCalibrationConfigWidget.h \
    ../src/tooladapters/CalibrationTransformAdapter.h \
    ../src/toolcore/ToolAdapter.h \
    ../src/toolcore/ToolConfig.h \
    ../src/toolcore/ToolRequest.h \
    ../src/toolcore/ToolResult.h \
    ../src/algorithms/location/CalibrationTransformHalconRunner.h
