QT += core widgets
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = registered_classification_detection_dialog_smoke

BUILD_ROOT = $$_PRO_FILE_PWD_/../build/smoke/registered_classification_detection_dialog
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

LIBS += -L$$OPENCV_ROOT/lib
LIBS += -Wl,-rpath,$$OPENCV_ROOT/lib
LIBS += -lopencv_core -lopencv_imgproc -lopencv_videoio -ldl

SOURCES += \
    registered_classification_detection_dialog_smoke.cpp \
    registered_classification_dialog_plan_stub.cpp \
    ../src/RegisteredClassificationDetectionDialog.cpp \
    ../src/RegisteredClassificationDetectionTrainingDialog.cpp \
    ../src/RegisteredClassificationTrainingDialog.cpp \
    ../src/RegisteredClassificationModelManagementDialog.cpp \
    ../src/ToolLibraryDialog.cpp \
    ../src/frame/CameraFrameProvider.cpp \
    ../src/frame/FrameViewHelper.cpp \
    ../src/frame/MatImageConverter.cpp \
    ../src/frame/ReferenceImageProvider.cpp \
    ../src/toolcore/PositionCorrection.cpp \
    ../src/algorithms/halcon/HalconRuntimePaths.cpp

HEADERS += \
    ../src/RegisteredClassificationDetectionDialog.h \
    ../src/RegisteredClassificationDetectionTrainingDialog.h \
    ../src/RegisteredClassificationTrainingDialog.h \
    ../src/RegisteredClassificationModelManagementDialog.h \
    ../src/ToolLibraryDialog.h \
    ../src/PlanDialogUtils.h \
    ../src/frame/CameraFrameProvider.h \
    ../src/frame/FrameViewHelper.h \
    ../src/frame/MatImageConverter.h \
    ../src/frame/ReferenceImageProvider.h \
    ../src/toolcore/ToolConfig.h \
    ../src/toolcore/ToolOverlay.h \
    ../src/toolcore/PositionCorrection.h \
    ../src/toolcore/ToolPreviewSnapshot.h \
    ../src/toolcore/ToolRequest.h \
    ../src/toolcore/ToolResult.h \
    ../src/toolcore/ToolTypes.h

FORMS += \
    ../ui/ToolLibraryDialog.ui

RESOURCES += \
    ../resources/resources.qrc
