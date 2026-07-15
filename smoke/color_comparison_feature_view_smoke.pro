QT += core gui widgets
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = color_comparison_feature_view_smoke

BUILD_ROOT = $$_PRO_FILE_PWD_/../build/smoke/color_comparison_feature_view
DESTDIR = $$BUILD_ROOT/bin
OBJECTS_DIR = $$BUILD_ROOT/obj
MOC_DIR = $$BUILD_ROOT/moc
RCC_DIR = $$BUILD_ROOT/rcc
UI_DIR = $$BUILD_ROOT/ui
system(mkdir -p $$DESTDIR $$OBJECTS_DIR $$MOC_DIR $$RCC_DIR $$UI_DIR)

INCLUDEPATH += ../src

SOURCES += \
    color_comparison_feature_view_smoke.cpp \
    ../src/ColorComparisonFeatureView.cpp

HEADERS += ../src/ColorComparisonFeatureView.h
