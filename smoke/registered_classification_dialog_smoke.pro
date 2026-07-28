QT += core widgets
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = registered_classification_dialog_smoke

BUILD_ROOT = $$_PRO_FILE_PWD_/../build/smoke/registered_classification_dialog
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

LIBS += -L$$OPENCV_ROOT/lib
LIBS += -Wl,-rpath,$$OPENCV_ROOT/lib
LIBS += -lopencv_core -lopencv_imgproc -lopencv_imgcodecs -lopencv_videoio -ldl
LIBS += -L$$HALCON_ROOT/lib/x64-linux
LIBS += -Wl,-rpath,$$HALCON_ROOT/lib/x64-linux
LIBS += -lhalconcpp -lhalcon

SOURCES += \
    registered_classification_dialog_smoke.cpp \
    registered_classification_dialog_plan_stub.cpp \
    ../src/RegisteredClassificationDialog.cpp \
    ../src/RegisteredClassificationTrainingDialog.cpp \
    ../src/RegisteredClassificationModelManagementDialog.cpp \
    ../src/frame/CameraFrameProvider.cpp \
    ../src/frame/FrameInputMetadata.cpp \
    ../src/frame/FrameViewHelper.cpp \
    ../src/frame/MatImageConverter.cpp \
    ../src/frame/ReferenceImageProvider.cpp \
    ../src/toolcore/PositionCorrection.cpp \
    ../src/toolcore/PositionCorrectionConsumer.cpp \
    ../src/toolcore/PositionCorrectionTransform.cpp \
    ../src/toolcore/ToolEngine.cpp \
    ../src/algorithms/location/PositionCorrectionHalconTransform.cpp \
    ../src/tooladapters/TemplateLocationAdapter.cpp \
    ../src/tooladapters/PositionCorrectionAdapter.cpp \
    ../src/tooladapters/RegisteredClassificationAdapter.cpp \
    ../src/algorithms/location/TemplateLocationHalconRunner.cpp \
    ../src/algorithms/location/PositionCorrectionHalconRunner.cpp \
    ../src/algorithms/recognition/RegisteredClassificationFeatureSpace.cpp \
    ../src/algorithms/recognition/RegisteredClassificationKnnRuntime.cpp \
    ../src/algorithms/recognition/RegisteredClassificationModelPackage.cpp \
    ../src/algorithms/recognition/RegisteredClassificationTrainingSession.cpp \
    ../src/algorithms/recognition/RegisteredClassificationFeatureExtractor.cpp \
    ../src/algorithms/recognition/RegisteredClassificationTrainingRunner.cpp \
    ../src/algorithms/recognition/RegisteredClassificationHalconRunner.cpp \
    ../src/algorithms/halcon/HalconRuntimePaths.cpp

HEADERS += \
    ../src/RegisteredClassificationDialog.h \
    ../src/RegisteredClassificationTrainingDialog.h \
    ../src/RegisteredClassificationModelManagementDialog.h \
    ../src/PlanDialogUtils.h \
    ../src/frame/CameraFrameProvider.h \
    ../src/frame/FrameInputMetadata.h \
    ../src/frame/FrameViewHelper.h \
    ../src/frame/MatImageConverter.h \
    ../src/frame/ReferenceImageProvider.h \
    ../src/tooladapters/RegisteredClassificationAdapter.h \
    ../src/algorithms/recognition/RegisteredClassificationFeatureSpace.h \
    ../src/algorithms/recognition/RegisteredClassificationKnnRuntime.h \
    ../src/algorithms/recognition/RegisteredClassificationModelPackage.h \
    ../src/algorithms/recognition/RegisteredClassificationTrainingSession.h \
    ../src/algorithms/recognition/RegisteredClassificationFeatureExtractor.h \
    ../src/algorithms/recognition/RegisteredClassificationTrainingRunner.h \
    ../src/algorithms/recognition/RegisteredClassificationHalconRunner.h \
    ../src/algorithms/halcon/HalconRuntimePaths.h \
    ../src/toolcore/ToolAdapter.h \
    ../src/toolcore/ToolConfig.h \
    ../src/toolcore/ToolOverlay.h \
    ../src/toolcore/PositionCorrection.h \
    ../src/toolcore/PositionCorrectionConsumer.h \
    ../src/toolcore/PositionCorrectionTransform.h \
    ../src/toolcore/ToolEngine.h \
    ../src/algorithms/location/PositionCorrectionHalconTransform.h \
    ../src/tooladapters/TemplateLocationAdapter.h \
    ../src/tooladapters/PositionCorrectionAdapter.h \
    ../src/algorithms/location/TemplateLocationHalconRunner.h \
    ../src/algorithms/location/PositionCorrectionHalconRunner.h \
    ../src/toolcore/ToolPreviewSnapshot.h \
    ../src/toolcore/ToolRequest.h \
    ../src/toolcore/ToolResult.h \
    ../src/toolcore/ToolTypes.h
