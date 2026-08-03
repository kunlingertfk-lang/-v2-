QT += core gui widgets
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = position_correction_ui_smoke

isEmpty(BUILD_ROOT): BUILD_ROOT = $$_PRO_FILE_PWD_/../build/smoke/position_correction_ui
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
LIBS += -L$$OPENCV_ROOT/lib -Wl,-rpath,$$OPENCV_ROOT/lib \
    -lopencv_core -lopencv_imgproc -lopencv_imgcodecs -lopencv_videoio

HALCONROOT = $$(HALCONROOT)
isEmpty(HALCONROOT) {
    HALCONROOT = /opt/halcon
}
HALCON_LIBDIR = $$HALCONROOT/lib/x64-linux
INCLUDEPATH += $$HALCONROOT/include $$HALCONROOT/include/halconcpp
LIBS += -L$$HALCON_LIBDIR -Wl,-rpath,$$HALCON_LIBDIR -lhalconcpp -lhalcon -ldl

SOURCES += \
    position_correction_ui_smoke.cpp \
    ../src/PositionCorrectionDialog.cpp \
    ../src/ToolLibraryDialog.cpp \
    ../src/algorithms/halcon/HalconRuntimePaths.cpp \
    ../src/algorithms/location/PositionCorrectionHalconRunner.cpp \
    ../src/algorithms/location/TemplateLocationHalconRunner.cpp \
    ../src/frame/CameraFrameProvider.cpp \
    ../src/frame/FrameInputMetadata.cpp \
    ../src/frame/FramePixelProbe.cpp \
    ../src/frame/FrameViewHelper.cpp \
    ../src/frame/MatImageConverter.cpp \
    ../src/frame/ReferenceImageProvider.cpp \
    ../src/tooladapters/PositionCorrectionAdapter.cpp \
    ../src/tooladapters/TemplateLocationAdapter.cpp \
    ../src/toolcore/PositionCorrection.cpp \
    ../src/toolcore/ToolEngine.cpp

HEADERS += \
    ../src/PositionCorrectionDialog.h \
    ../src/ToolLibraryDialog.h \
    ../src/algorithms/halcon/HalconRuntimePaths.h \
    ../src/algorithms/location/PositionCorrectionHalconRunner.h \
    ../src/algorithms/location/TemplateLocationHalconRunner.h \
    ../src/frame/CameraFrameProvider.h \
    ../src/frame/FrameInputMetadata.h \
    ../src/frame/FramePixelProbe.h \
    ../src/frame/FrameViewHelper.h \
    ../src/frame/MatImageConverter.h \
    ../src/frame/ReferenceImageProvider.h \
    ../src/tooladapters/PositionCorrectionAdapter.h \
    ../src/tooladapters/TemplateLocationAdapter.h \
    ../src/toolcore/PositionCorrection.h \
    ../src/toolcore/ToolEngine.h \
    ../src/toolcore/ToolConfig.h \
    ../src/toolcore/ToolPreviewSnapshot.h \
    ../src/toolcore/ToolTypes.h

FORMS += \
    ../ui/PositionCorrectionDialog.ui \
    ../ui/ToolLibraryDialog.ui

RESOURCES += ../resources/resources.qrc
