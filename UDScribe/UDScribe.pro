# UDScribe: the diagnostic description editor of AutoForge.

QT += core gui widgets

CONFIG += c++17

TARGET = UDScribe
TEMPLATE = app

SOURCES += \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    mainwindow.h

# The diagnostic data and .uscxml reading/writing come from the usccore library.
include(../usccore/usccore.pri)
