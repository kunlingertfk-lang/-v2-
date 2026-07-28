QT += core gui widgets
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = template_location_ui_smoke

isEmpty(BUILD_ROOT) {
    BUILD_ROOT = $$_PRO_FILE_PWD_/../build/smoke/template_location_ui
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
    template_location_ui_smoke.cpp \
    ../src/TemplateLocationDialog.cpp \
    ../src/ToolLibraryDialog.cpp \
    ../src/algorithms/halcon/HalconRuntimePaths.cpp \
    ../src/algorithms/location/TemplateLocationHalconRunner.cpp \
    ../src/algorithms/location/PositionCorrectionHalconTransform.cpp \
    ../src/algorithms/location/PositionCorrectionHalconRunner.cpp \
    ../src/algorithms/presence/PatternPresenceHalconApi.cpp \
    ../src/algorithms/presence/PatternPresenceAutoModelDomain.cpp \
    ../src/algorithms/presence/PatternPresenceHalconRunner.cpp \
    ../src/frame/CameraFrameProvider.cpp \
    ../src/frame/FrameInputMetadata.cpp \
    ../src/frame/FrameViewHelper.cpp \
    ../src/frame/MatImageConverter.cpp \
    ../src/frame/ReferenceImageProvider.cpp \
    ../src/tooladapters/TemplateLocationAdapter.cpp \
    ../src/toolcore/PositionCorrection.cpp \
    ../src/toolcore/PositionCorrectionTransform.cpp \
    ../src/toolcore/ToolEngine.cpp

HEADERS += \
    ../src/TemplateLocationDialog.h \
    ../src/algorithms/location/PositionCorrectionHalconTransform.h \
    ../src/algorithms/location/PositionCorrectionHalconRunner.h \
    ../src/ToolLibraryDialog.h \
    ../src/frame/CameraFrameProvider.h \
    ../src/frame/FrameViewHelper.h \
    ../src/frame/ReferenceImageProvider.h \
    ../src/tooladapters/TemplateLocationAdapter.h \
    ../src/toolcore/ToolConfig.h \
    ../src/toolcore/ToolEngine.h \
    ../src/toolcore/ToolPreviewSnapshot.h \
    ../src/toolcore/PositionCorrection.h \
    ../src/toolcore/PositionCorrectionTransform.h \
    ../src/toolcore/ToolTypes.h

FORMS += \
    ../ui/TemplateLocationDialog.ui \
    ../ui/ToolLibraryDialog.ui

RESOURCES += ../resources/resources.qrc

LIBS += -L$$OPENCV_ROOT/lib -Wl,-rpath,$$OPENCV_ROOT/lib
LIBS += -L$$HALCON_ROOT/lib/x64-linux -Wl,-rpath,$$HALCON_ROOT/lib/x64-linux
LIBS += -lopencv_core -lopencv_imgproc -lopencv_imgcodecs -lopencv_videoio -lhalconcpp -lhalcon -ldl
