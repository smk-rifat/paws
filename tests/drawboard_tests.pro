QT += core gui qml quick quickcontrols2 svg testlib

CONFIG += c++17 testcase
TARGET = drawboard_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += \
    ../src/types.h \
    ../src/geometry.h \
    ../src/document.h \
    ../src/history.h \
    ../src/theme.h \
    ../src/shortcuts.h \
    ../src/canvasitem.h \
    ../src/exporter.h \
    ../src/backend.h

SOURCES += \
    ../src/geometry.cpp \
    ../src/document.cpp \
    ../src/history.cpp \
    ../src/theme.cpp \
    ../src/shortcuts.cpp \
    ../src/canvasitem.cpp \
    ../src/exporter.cpp \
    ../src/backend.cpp \
    drawboard_tests.cpp

RESOURCES += ../src/resources.qrc

