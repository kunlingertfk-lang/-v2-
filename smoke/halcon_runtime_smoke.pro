QT += core gui
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = halcon_runtime_smoke

INCLUDEPATH += ../src
INCLUDEPATH += /usr/include/opencv4
HALCON_ROOT = $$(HALCONROOT)
!exists($$HALCON_ROOT/include/HalconC.h) {
    HALCON_ROOT = /home/superhe/桌面/som-halcon/repository/packages.mvtec.com/halcon/halcon-24.11-progress-steady/halcon-24.11.2.0-development_general-x64-linux
}
INCLUDEPATH += $$HALCON_ROOT/include

SOURCES += \
    halcon_runtime_smoke.cpp \
    ../src/algorithms/halcon/HalconRuntimePaths.cpp \
    ../src/algorithms/ocr/OcrHalconRunner.cpp \
    ../src/algorithms/presence/PatternPresenceHalconApi.cpp \
    ../src/algorithms/presence/PatternPresenceAutoModelDomain.cpp \
    ../src/algorithms/presence/PatternPresenceHalconRunner.cpp \
    ../src/algorithms/presence/BlobPresenceHalconRunner.cpp \
    ../src/algorithms/presence/CirclePresenceHalconRunner.cpp \
    ../src/algorithms/presence/EdgePresenceHalconRunner.cpp \
    ../src/algorithms/presence/LinePresenceHalconRunner.cpp

LIBS += -lopencv_core -lopencv_imgproc -lopencv_imgcodecs -ldl
