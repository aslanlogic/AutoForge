QT += core gui widgets network

CONFIG += c++17

TARGET = DiagProbe
TEMPLATE = app

SOURCES += \
    decoding.cpp \
    diagnosticconsole.cpp \
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
    ecustatewindow.h \
    generatorwindow.h \
    graphwindow.h \
    mainwindow.h \
    tester.h \
    tracewindow.h \
    udpbus.h

# The Diagnostic console uses the same icons as UDScribe's tree.
RESOURCES += ../UDScribe/resources.qrc

include(../usccore/usccore.pri)
include(../ffcore/ffcore.pri)
