QT += core
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = color_recognition_gmm_detection_smoke

INCLUDEPATH += ../src /opt/halcon/include
CONFIG += link_pkgconfig
PKGCONFIG += opencv4

SOURCES += color_recognition_gmm_detection_smoke.cpp \
           ../src/algorithms/halcon/HalconRuntimePaths.cpp \
           ../src/algorithms/recognition/ColorRecognitionGmmHalconBackend.cpp \
           ../src/algorithms/location/PositionCorrectionHalconTransform.cpp \
           ../src/toolcore/PositionCorrectionTransform.cpp

HEADERS += ../src/algorithms/halcon/HalconRuntimePaths.h \
           ../src/algorithms/recognition/ColorRecognitionHalconRunner.h \
           ../src/algorithms/recognition/ColorRecognitionGmmHalconBackend.h \
           ../src/algorithms/location/PositionCorrectionHalconTransform.h \
           ../src/toolcore/PositionCorrectionTransform.h \
           ../src/toolcore/ToolOverlay.h

LIBS += -ldl
