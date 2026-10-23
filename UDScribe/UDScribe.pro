# UDScribe: the diagnostic description editor of AutoForge.

QT += core gui widgets

CONFIG += c++17

TARGET = UDScribe
TEMPLATE = app

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    navigationmodel.cpp \
    securityleveldialog.cpp

HEADERS += \
    mainwindow.h \
    navigationmodel.h \
    securityleveldialog.h

RESOURCES += \
    resources.qrc

# The diagnostic data and .uscxml reading/writing come from the usccore library.
include(../usccore/usccore.pri)
