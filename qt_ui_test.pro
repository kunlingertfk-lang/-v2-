QT += widgets
QT += concurrent
QT += multimedia multimediawidgets
CONFIG += c++17
TEMPLATE = app
TARGET = qt_ui_test

isEmpty(BUILD_ROOT): BUILD_ROOT = $$_PRO_FILE_PWD_/build/qt_ui_test
DESTDIR = $$BUILD_ROOT/bin
OBJECTS_DIR = $$BUILD_ROOT/obj
MOC_DIR = $$BUILD_ROOT/moc
RCC_DIR = $$BUILD_ROOT/rcc
UI_DIR = $$BUILD_ROOT/ui
system(mkdir -p $$DESTDIR $$OBJECTS_DIR $$MOC_DIR $$RCC_DIR $$UI_DIR)

INCLUDEPATH += src
exists($$_PRO_FILE_PWD_/local_paths.pri): include($$_PRO_FILE_PWD_/local_paths.pri)
OPENCV_ROOT_ENV = $$(OPENCV_ROOT)
OPENCV_LIB_DIR_ENV = $$(OPENCV_LIB_DIR)
HALCON_ROOT_ENV = $$(HALCONROOT)
!isEmpty(OPENCV_ROOT_ENV): OPENCV_ROOT = $$OPENCV_ROOT_ENV
!isEmpty(OPENCV_LIB_DIR_ENV): OPENCV_LIB_DIR = $$OPENCV_LIB_DIR_ENV
!isEmpty(HALCON_ROOT_ENV): HALCON_ROOT = $$HALCON_ROOT_ENV
include(qmake/opencv.pri)
include(qmake/halcon_20_11.pri)
INCLUDEPATH += $$HALCON_ROOT/include/halconcpp
LIBS += -L$$HALCON_ROOT/lib/x64-linux -Wl,-rpath,$$HALCON_ROOT/lib/x64-linux -lhalconcpp -lhalcon

SOURCES += \
    src/main.cpp \
    src/LoginWindow.cpp \
    src/MainWindow.cpp \
    src/SchemeStore.cpp \
    src/PlanDialogUtils.cpp \
    src/SchemeSetupWindow.cpp \
    src/CameraParamsDialog.cpp \
    src/CalibrationTransformDialog.cpp \
    src/QuickCalibrationWizard.cpp \
    src/ReferenceImageDialog.cpp \
    src/PositionCorrectionDialog.cpp \
    src/PositionCorrectionDialogTestHelper.cpp \
    src/TemplateLocationDialog.cpp \
    src/ToolLibraryDialog.cpp \
    src/CharacterRecognitionDialog.cpp \
    src/ColorRecognitionDialog.cpp \
    src/ColorComparisonDialog.cpp \
    src/ColorComparisonFeatureView.cpp \
    src/RegisteredClassificationDialog.cpp \
    src/RegisteredClassificationDetectionDialog.cpp \
    src/RegisteredClassificationDetectionTrainingDialog.cpp \
    src/RegisteredClassificationTrainingDialog.cpp \
    src/RegisteredClassificationModelManagementDialog.cpp \
    src/ColorTemplateDialog.cpp \
    src/ClassificationDialog.cpp \
    src/ObjectDetectionDialog.cpp \
    src/PatternPresenceDialog.cpp \
    src/BlobPresenceDialog.cpp \
    src/CirclePresenceDialog.cpp \
    src/EdgePresenceDialog.cpp \
    src/LinePresenceDialog.cpp \
    src/ContourPresenceDialog.cpp \
    src/ToolsDialog.cpp \
    src/OutputDialog.cpp \
    src/frame/CameraFrameProvider.cpp \
    src/frame/FrameInputMetadata.cpp \
    src/frame/FramePixelProbe.cpp \
    src/frame/FrameViewHelper.cpp \
    src/frame/MatImageConverter.cpp \
    src/frame/ReferenceImageProvider.cpp \
    src/toolcore/PositionCorrection.cpp \
    src/toolcore/PositionCorrectionConsumer.cpp \
    src/toolcore/PositionCorrectionTransform.cpp \
    src/calibration/CalibrationModel.cpp \
    src/calibration/CalibrationSolver.cpp \
    src/calibration/CalibrationFileLoader.cpp \
    src/calibration/CalibrationCommunicationProtocol.cpp \
    src/calibration/CalibrationMethodRegistry.cpp \
    src/calibration/NPointCalibrationConfigWidget.cpp \
    src/toolcore/ToolEngine.cpp \
    src/tooladapters/OcrAdapter.cpp \
    src/tooladapters/ColorRecognitionAdapter.cpp \
    src/tooladapters/ColorComparisonAdapter.cpp \
    src/tooladapters/RegisteredClassificationAdapter.cpp \
    src/tooladapters/PatternPresenceAdapter.cpp \
    src/tooladapters/TemplateLocationAdapter.cpp \
    src/tooladapters/PositionCorrectionAdapter.cpp \
    src/tooladapters/CalibrationTransformAdapter.cpp \
    src/tooladapters/BlobPresenceAdapter.cpp \
    src/tooladapters/CirclePresenceAdapter.cpp \
    src/tooladapters/EdgePresenceAdapter.cpp \
    src/tooladapters/LinePresenceAdapter.cpp \
    src/tooladapters/ContourPresenceAdapter.cpp \
    src/tooladapters/AiDetectionAdapter.cpp \
    src/algorithms/halcon/HalconRuntimePaths.cpp \
    src/algorithms/ocr/OcrHalconRunner.cpp \
    src/algorithms/recognition/ColorRecognitionGmmHalconBackend.cpp \
    src/algorithms/recognition/ColorRecognitionHalconRunner.cpp \
    src/algorithms/recognition/ColorComparisonModel.cpp \
    src/algorithms/recognition/ColorComparisonHalconRunner.cpp \
    src/algorithms/recognition/RegisteredClassificationFeatureSpace.cpp \
    src/algorithms/recognition/RegisteredClassificationEmbeddingModelProvider.cpp \
    src/algorithms/recognition/RegisteredClassificationDlCapability.cpp \
    src/algorithms/recognition/RegisteredClassificationModelPackage.cpp \
    src/algorithms/recognition/RegisteredClassificationTrainingSession.cpp \
    src/algorithms/recognition/RegisteredClassificationFeatureExtractor.cpp \
    src/algorithms/recognition/RegisteredClassificationKnnRuntime.cpp \
    src/algorithms/recognition/RegisteredClassificationTrainingRunner.cpp \
    src/algorithms/recognition/RegisteredClassificationHalconRunner.cpp \
    src/algorithms/ai/AiDetectionRunner.cpp \
    src/algorithms/presence/PatternPresenceHalconApi.cpp \
    src/algorithms/presence/PatternPresenceAutoModelDomain.cpp \
    src/algorithms/presence/PatternPresenceHalconRunner.cpp \
    src/algorithms/location/TemplateLocationHalconRunner.cpp \
    src/algorithms/location/PositionCorrectionHalconRunner.cpp \
    src/algorithms/location/PositionCorrectionHalconTransform.cpp \
    src/algorithms/location/CalibrationTransformHalconRunner.cpp \
    src/algorithms/presence/BlobPresenceHalconRunner.cpp \
    src/algorithms/presence/CirclePresenceHalconRunner.cpp \
    src/algorithms/presence/EdgePresenceHalconRunner.cpp \
    src/algorithms/presence/LinePresenceHalconRunner.cpp \
    src/algorithms/presence/ContourPresenceHalconRunner.cpp

HEADERS += \
    src/LoginWindow.h \
    src/MainWindow.h \
    src/SchemeStore.h \
    src/PlanDialogUtils.h \
    src/SchemeSetupWindow.h \
    src/UiStyleRoles.h \
    src/CameraParamsDialog.h \
    src/CalibrationTransformDialog.h \
    src/QuickCalibrationWizard.h \
    src/ReferenceImageDialog.h \
    src/PositionCorrectionDialog.h \
    src/PositionCorrectionDialogTestHelper.h \
    src/TemplateLocationDialog.h \
    src/ToolLibraryDialog.h \
    src/CharacterRecognitionDialog.h \
    src/ColorRecognitionDialog.h \
    src/ColorComparisonDialog.h \
    src/ColorComparisonFeatureView.h \
    src/RegisteredClassificationDialog.h \
    src/RegisteredClassificationDetectionDialog.h \
    src/RegisteredClassificationDetectionTrainingDialog.h \
    src/RegisteredClassificationTrainingDialog.h \
    src/RegisteredClassificationModelManagementDialog.h \
    src/ColorTemplateDialog.h \
    src/ClassificationDialog.h \
    src/ObjectDetectionDialog.h \
    src/PatternPresenceDialog.h \
    src/BlobPresenceDialog.h \
    src/CirclePresenceDialog.h \
    src/EdgePresenceDialog.h \
    src/LinePresenceDialog.h \
    src/ContourPresenceDialog.h \
    src/ToolsDialog.h \
    src/OutputDialog.h \
    src/frame/CameraFrameProvider.h \
    src/frame/FrameInputMetadata.h \
    src/frame/FramePixelProbe.h \
    src/frame/FrameViewHelper.h \
    src/frame/MatImageConverter.h \
    src/frame/ReferenceImageProvider.h \
    src/toolcore/ToolTypes.h \
    src/toolcore/ToolConfig.h \
    src/toolcore/ToolRequest.h \
    src/toolcore/ToolResult.h \
    src/toolcore/ToolOverlay.h \
    src/toolcore/PositionCorrection.h \
    src/toolcore/PositionCorrectionConsumer.h \
    src/toolcore/PositionCorrectionTransform.h \
    src/calibration/CalibrationModel.h \
    src/calibration/CalibrationSolver.h \
    src/calibration/CalibrationFileLoader.h \
    src/calibration/CalibrationCommunicationProtocol.h \
    src/calibration/CalibrationMethodRegistry.h \
    src/calibration/NPointCalibrationConfigWidget.h \
    src/toolcore/ToolPreviewSnapshot.h \
    src/toolcore/ToolAdapter.h \
    src/toolcore/ToolEngine.h \
    src/tooladapters/OcrAdapter.h \
    src/tooladapters/ColorRecognitionAdapter.h \
    src/tooladapters/ColorComparisonAdapter.h \
    src/tooladapters/RegisteredClassificationAdapter.h \
    src/tooladapters/PatternPresenceAdapter.h \
    src/tooladapters/TemplateLocationAdapter.h \
    src/tooladapters/PositionCorrectionAdapter.h \
    src/tooladapters/CalibrationTransformAdapter.h \
    src/tooladapters/BlobPresenceAdapter.h \
    src/tooladapters/CirclePresenceAdapter.h \
    src/tooladapters/EdgePresenceAdapter.h \
    src/tooladapters/LinePresenceAdapter.h \
    src/tooladapters/ContourPresenceAdapter.h \
    src/tooladapters/AiDetectionAdapter.h \
    src/algorithms/halcon/HalconRuntimePaths.h \
    src/algorithms/ocr/OcrHalconRunner.h \
    src/algorithms/recognition/ColorRecognitionGmmHalconBackend.h \
    src/algorithms/recognition/ColorRecognitionHalconRunner.h \
    src/algorithms/recognition/ColorComparisonModel.h \
    src/algorithms/recognition/ColorComparisonHalconRunner.h \
    src/algorithms/recognition/RegisteredClassificationFeatureSpace.h \
    src/algorithms/recognition/RegisteredClassificationEmbeddingModelProvider.h \
    src/algorithms/recognition/RegisteredClassificationDlCapability.h \
    src/algorithms/recognition/RegisteredClassificationModelPackage.h \
    src/algorithms/recognition/RegisteredClassificationTrainingSession.h \
    src/algorithms/recognition/RegisteredClassificationFeatureExtractor.h \
    src/algorithms/recognition/RegisteredClassificationKnnRuntime.h \
    src/algorithms/recognition/RegisteredClassificationTrainingRunner.h \
    src/algorithms/recognition/RegisteredClassificationHalconRunner.h \
    src/algorithms/ai/AiDetectionRunner.h \
    src/algorithms/presence/PatternPresenceHalconApi.h \
    src/algorithms/presence/PatternPresenceAutoModelDomain.h \
    src/algorithms/presence/PatternPresenceHalconRunner.h \
    src/algorithms/location/TemplateLocationHalconRunner.h \
    src/algorithms/location/PositionCorrectionHalconTransform.h \
    src/algorithms/location/CalibrationTransformHalconRunner.h \
    src/algorithms/location/PositionCorrectionHalconRunner.h \
    src/algorithms/presence/BlobPresenceHalconRunner.h \
    src/algorithms/presence/CirclePresenceHalconRunner.h \
    src/algorithms/presence/EdgePresenceHalconRunner.h \
    src/algorithms/presence/LinePresenceHalconRunner.h \
    src/algorithms/presence/ContourPresenceHalconRunner.h

FORMS += \
    ui/LoginWindow.ui \
    ui/MainWindow.ui \
    ui/SchemeSetupWindow.ui \
    ui/CameraParamsDialog.ui \
    ui/CalibrationTransformDialog.ui \
    ui/ReferenceImageDialog.ui \
    ui/PositionCorrectionDialog.ui \
    ui/TemplateLocationDialog.ui \
    ui/ToolLibraryDialog.ui \
    ui/CharacterRecognitionDialog.ui \
    ui/ColorRecognitionDialog.ui \
    ui/ColorComparisonDialog.ui \
    ui/ColorTemplateDialog.ui \
    ui/RegisteredClassificationDialog.ui \
    ui/RegisteredClassificationTrainingDialog.ui \
    ui/RegisteredClassificationDetectionDialog.ui \
    ui/RegisteredClassificationDetectionTrainingDialog.ui \
    ui/RegisteredClassificationModelManagementDialog.ui \
    ui/CreateDatasetDialog.ui \
    ui/ClassificationDialog.ui \
    ui/ObjectDetectionDialog.ui \
    ui/PatternPresenceDialog.ui \
    ui/BlobPresenceDialog.ui \
    ui/CirclePresenceDialog.ui \
    ui/EdgePresenceDialog.ui \
    ui/LinePresenceDialog.ui \
    ui/ContourPresenceDialog.ui \
    ui/ToolsDialog.ui \
    ui/OutputDialog.ui

RESOURCES += \
    resources/resources.qrc

LIBS += -ldl
