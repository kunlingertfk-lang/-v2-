QT += core gui
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = presence_ocr_position_correction_contract_smoke

isEmpty(BUILD_ROOT): BUILD_ROOT = $$_PRO_FILE_PWD_/../build/smoke/presence_ocr_position_correction_contract
DESTDIR = $$BUILD_ROOT/bin
OBJECTS_DIR = $$BUILD_ROOT/obj
MOC_DIR = $$BUILD_ROOT/moc
RCC_DIR = $$BUILD_ROOT/rcc
UI_DIR = $$BUILD_ROOT/ui
system(mkdir -p $$DESTDIR $$OBJECTS_DIR $$MOC_DIR $$RCC_DIR $$UI_DIR)

INCLUDEPATH += ../src
OPENCV_ROOT = $$(OPENCV_ROOT)
isEmpty(OPENCV_ROOT): OPENCV_ROOT = /usr
INCLUDEPATH += $$OPENCV_ROOT/include/opencv4
HALCON_ROOT = $$(HALCONROOT)
isEmpty(HALCON_ROOT): HALCON_ROOT = /opt/halcon
INCLUDEPATH += $$HALCON_ROOT/include $$HALCON_ROOT/include/halconcpp

SOURCES += \
    presence_ocr_position_correction_contract_smoke.cpp \
    ../src/tooladapters/PatternPresenceAdapter.cpp \
    ../src/tooladapters/EdgePresenceAdapter.cpp \
    ../src/tooladapters/LinePresenceAdapter.cpp \
    ../src/tooladapters/ContourPresenceAdapter.cpp \
    ../src/tooladapters/OcrAdapter.cpp \
    ../src/toolcore/PositionCorrection.cpp \
    ../src/toolcore/PositionCorrectionConsumer.cpp \
    ../src/toolcore/PositionCorrectionTransform.cpp \
    ../src/algorithms/halcon/HalconRuntimePaths.cpp \
    ../src/algorithms/location/PositionCorrectionHalconTransform.cpp \
    ../src/algorithms/presence/PatternPresenceHalconApi.cpp \
    ../src/algorithms/presence/PatternPresenceAutoModelDomain.cpp \
    ../src/algorithms/presence/PatternPresenceHalconRunner.cpp \
    ../src/algorithms/presence/EdgePresenceHalconRunner.cpp \
    ../src/algorithms/presence/LinePresenceHalconRunner.cpp \
    ../src/algorithms/presence/ContourPresenceHalconRunner.cpp \
    ../src/algorithms/ocr/OcrHalconRunner.cpp

LIBS += -lopencv_core -lopencv_imgproc -lopencv_imgcodecs -ldl
LIBS += -L$$HALCON_ROOT/lib/x64-linux -Wl,-rpath,$$HALCON_ROOT/lib/x64-linux
LIBS += -lhalconcpp -lhalcon
