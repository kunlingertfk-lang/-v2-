QT += widgets
QT += multimedia multimediawidgets
CONFIG += c++17
TEMPLATE = app
TARGET = qt_ui_test

INCLUDEPATH += src
INCLUDEPATH += /usr/include/opencv4

SOURCES += \
    src/main.cpp \
    src/LoginWindow.cpp \
    src/MainWindow.cpp \
    src/PlanDialogUtils.cpp \
    src/CameraParamsDialog.cpp \
    src/ReferenceImageDialog.cpp \
    src/ToolLibraryDialog.cpp \
    src/CharacterRecognitionDialog.cpp \
    src/ToolsDialog.cpp \
    src/OutputDialog.cpp

HEADERS += \
    src/LoginWindow.h \
    src/MainWindow.h \
    src/PlanDialogUtils.h \
    src/CameraParamsDialog.h \
    src/ReferenceImageDialog.h \
    src/ToolLibraryDialog.h \
    src/CharacterRecognitionDialog.h \
    src/ToolsDialog.h \
    src/OutputDialog.h

FORMS += \
    ui/LoginWindow.ui \
    ui/MainWindow.ui \
    ui/CameraParamsDialog.ui \
    ui/ReferenceImageDialog.ui \
    ui/ToolLibraryDialog.ui \
    ui/CharacterRecognitionDialog.ui \
    ui/ToolsDialog.ui \
    ui/OutputDialog.ui

RESOURCES += \
    resources/resources.qrc

# OpenCV库链接（移植自旧项目 qtt5_project_bak_327_10nrs_260328he/qtt5.pro）
LIBS += -lopencv_core -lopencv_imgproc -lopencv_highgui -lopencv_videoio -lopencv_imgcodecs
