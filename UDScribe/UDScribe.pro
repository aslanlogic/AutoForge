# UDScribe: the diagnostic description editor of AutoForge.

QT += core gui widgets

CONFIG += c++17

TARGET = UDScribe
TEMPLATE = app

SOURCES += \
    accessdialog.cpp \
    dataidentifierdialog.cpp \
    dataidentifierlayoutwidget.cpp \
    dataidentifierpropertieswidget.cpp \
    dataidentifiertablemodel.cpp \
    datatypedialog.cpp \
    datatypepropertieswidget.cpp \
    datatypetablemodel.cpp \
    main.cpp \
    mainwindow.cpp \
    navigationmodel.cpp \
    networkdialog.cpp \
    networkpropertieswidget.cpp \
    resettypedialog.cpp \
    resettypepropertieswidget.cpp \
    resettypetablemodel.cpp \
    securityleveldialog.cpp \
    securitylevelpropertieswidget.cpp \
    securityleveltablemodel.cpp \
    sessiondialog.cpp \
    sessionpropertieswidget.cpp \
    sessiontablemodel.cpp \
    valuetabledialog.cpp \
    valuetablepropertieswidget.cpp \
    valuetabletablemodel.cpp

HEADERS += \
    accessdialog.h \
    dataidentifierdialog.h \
    dataidentifierlayoutwidget.h \
    dataidentifierpropertieswidget.h \
    dataidentifiertablemodel.h \
    datatypedialog.h \
    datatypepropertieswidget.h \
    datatypetablemodel.h \
    mainwindow.h \
    navigationmodel.h \
    networkdialog.h \
    networkpropertieswidget.h \
    resettypedialog.h \
    resettypepropertieswidget.h \
    resettypetablemodel.h \
    propertytable.h \
    securityleveldialog.h \
    securitylevelpropertieswidget.h \
    securityleveltablemodel.h \
    sessiondialog.h \
    sessionpropertieswidget.h \
    sessiontablemodel.h \
    valuetabledialog.h \
    valuetablepropertieswidget.h \
    valuetabletablemodel.h

RESOURCES += \
    resources.qrc

# The diagnostic data and .uscxml reading/writing come from the usccore library.
include(../usccore/usccore.pri)
