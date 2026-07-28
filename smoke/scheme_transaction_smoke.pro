QT += core gui
CONFIG += console c++17 link_pkgconfig
CONFIG -= app_bundle
TEMPLATE = app
TARGET = scheme_transaction_smoke

isEmpty(BUILD_ROOT): BUILD_ROOT = $$_PRO_FILE_PWD_/../build/smoke/scheme_transaction
DESTDIR = $$BUILD_ROOT/bin
OBJECTS_DIR = $$BUILD_ROOT/obj
MOC_DIR = $$BUILD_ROOT/moc
RCC_DIR = $$BUILD_ROOT/rcc
UI_DIR = $$BUILD_ROOT/ui
system(mkdir -p $$DESTDIR $$OBJECTS_DIR $$MOC_DIR $$RCC_DIR $$UI_DIR)

INCLUDEPATH += ../src
PKGCONFIG += opencv4

SOURCES += \
    scheme_transaction_smoke.cpp \
    ../src/SchemeStore.cpp \
    ../src/frame/FrameInputMetadata.cpp \
    ../src/frame/MatImageConverter.cpp \
    ../src/frame/ReferenceImageProvider.cpp \
    ../src/toolcore/PositionCorrection.cpp

HEADERS += \
    ../src/SchemeStore.h \
    ../src/frame/FrameInputMetadata.h \
    ../src/frame/MatImageConverter.h \
    ../src/frame/ReferenceImageProvider.h \
    ../src/toolcore/PositionCorrection.h
