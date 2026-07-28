QT += core
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = registered_classification_dl_capability_smoke

BUILD_ROOT = $$_PRO_FILE_PWD_/../build/smoke/registered_classification_dl_capability
DESTDIR = $$BUILD_ROOT/bin
OBJECTS_DIR = $$BUILD_ROOT/obj
MOC_DIR = $$BUILD_ROOT/moc
RCC_DIR = $$BUILD_ROOT/rcc
UI_DIR = $$BUILD_ROOT/ui
system(mkdir -p $$DESTDIR $$OBJECTS_DIR $$MOC_DIR $$RCC_DIR $$UI_DIR)

INCLUDEPATH += ../src
include(../qmake/halcon_20_11.pri)
INCLUDEPATH += $$HALCON_ROOT/include/halconcpp
LIBS += -L$$HALCON_ROOT/lib/x64-linux
LIBS += -Wl,-rpath,$$HALCON_ROOT/lib/x64-linux
LIBS += -lhalconcpp -lhalcon

SOURCES += \
    registered_classification_dl_capability_smoke.cpp \
    ../src/algorithms/recognition/RegisteredClassificationEmbeddingModelProvider.cpp \
    ../src/algorithms/recognition/RegisteredClassificationDlCapability.cpp \
    ../src/algorithms/halcon/HalconRuntimePaths.cpp

HEADERS += \
    ../src/algorithms/recognition/RegisteredClassificationEmbeddingModelProvider.h \
    ../src/algorithms/recognition/RegisteredClassificationDlCapability.h \
    ../src/algorithms/halcon/HalconRuntimePaths.h
