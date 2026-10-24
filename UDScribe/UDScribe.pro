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
    securityleveltablemodel.cpp \
    sessiondialog.cpp \
    sessionpropertieswidget.cpp \
    sessiontablemodel.cpp

HEADERS += \
    mainwindow.h \
    navigationmodel.h \
    networkdialog.h \
    networkpropertieswidget.h \
    propertytable.h \
    securityleveldialog.h \
    securitylevelpropertieswidget.h \
    securityleveltablemodel.h \
    sessiondialog.h \
    sessionpropertieswidget.h \
    sessiontablemodel.h

RESOURCES += \
    resources.qrc

# The diagnostic data and .uscxml reading/writing come from the usccore library.
include(../usccore/usccore.pri)
