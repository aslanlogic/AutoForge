# UDScribe: the diagnostic description editor of AutoForge.

QT += core gui widgets

CONFIG += c++17

TARGET = UDScribe
TEMPLATE = app

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    navigationmodel.cpp \
    networkdialog.cpp \
    networkpropertieswidget.cpp \
    securityleveldialog.cpp \
    securitylevelpropertieswidget.cpp \
    securityleveltablemodel.cpp

HEADERS += \
    mainwindow.h \
    navigationmodel.h \
    networkdialog.h \
    networkpropertieswidget.h \
    propertytable.h \
    securityleveldialog.h \
    securitylevelpropertieswidget.h \
    securityleveltablemodel.h

RESOURCES += \
    resources.qrc

# The diagnostic data and .uscxml reading/writing come from the usccore library.
include(../usccore/usccore.pri)
