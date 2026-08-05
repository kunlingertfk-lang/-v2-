QT += widgets
CONFIG += c++17
CONFIG += link_pkgconfig
PKGCONFIG += opencv4
TEMPLATE = app
TARGET = dialog_lifecycle_smoke

BUILD_ROOT = $$PWD/../build/smoke/dialog_lifecycle
DESTDIR = $$BUILD_ROOT/bin
OBJECTS_DIR = $$BUILD_ROOT/obj
MOC_DIR = $$BUILD_ROOT/moc

INCLUDEPATH += $$PWD/../src
INCLUDEPATH += /opt/halcon/include /opt/halcon/include/halconcpp
QMAKE_CXXFLAGS += -ffunction-sections -fdata-sections
QMAKE_LFLAGS += -Wl,--gc-sections

SOURCES += \
    dialog_lifecycle_smoke.cpp \
    ../src/PlanDialogUtils.cpp

HEADERS += ../src/PlanDialogUtils.h
