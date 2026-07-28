QT += core
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = registered_classification_public_backbone_provision

BUILD_ROOT = $$_PRO_FILE_PWD_/../build/smoke/registered_classification_public_backbone_provision
DESTDIR = $$BUILD_ROOT/bin
OBJECTS_DIR = $$BUILD_ROOT/obj
MOC_DIR = $$BUILD_ROOT/moc
RCC_DIR = $$BUILD_ROOT/rcc
UI_DIR = $$BUILD_ROOT/ui
system(mkdir -p $$DESTDIR $$OBJECTS_DIR $$MOC_DIR $$RCC_DIR $$UI_DIR)

include(../qmake/halcon_20_11.pri)
INCLUDEPATH += $$HALCON_ROOT/include/halconcpp
INCLUDEPATH += $$HALCON_ROOT/include/hdevengine
LIBS += -L$$HALCON_ROOT/lib/x64-linux
LIBS += -Wl,-rpath,$$HALCON_ROOT/lib/x64-linux
LIBS += -lhdevenginecpp -lhalconcpp -lhalcon

SOURCES += registered_classification_public_backbone_provision.cpp
