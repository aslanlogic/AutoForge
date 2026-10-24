QT += core gui widgets network

CONFIG += c++17

TARGET = DiagProbe
TEMPLATE = app

# File → Open Examples loads the example files from here.
DEFINES += EXAMPLES_DIR=\\\"$$clean_path($$PWD/../examples)\\\"

SOURCES += \
    decoding.cpp \
    diagnosticconsole.cpp \
    ecusimulator.cpp \
    ecustatewindow.cpp \
    generatorwindow.cpp \
    graphwindow.cpp \
    main.cpp \
    mainwindow.cpp \
    tester.cpp \
    tracewindow.cpp \
    udpbus.cpp

HEADERS += \
    bitcodec.h \
    decoding.h \
    diagdefaults.h \
    diagnosticconsole.h \
    ecusimulator.h \
    ecustatewindow.h \
    generatorwindow.h \
    graphwindow.h \
    mainwindow.h \
    tester.h \
    tracewindow.h \
    udpbus.h

include(../usccore/usccore.pri)
include(../ffcore/ffcore.pri)
