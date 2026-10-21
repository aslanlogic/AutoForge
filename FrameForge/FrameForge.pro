# FrameForge: the message database editor of AutoForge.

QT += core gui widgets

CONFIG += c++17

TARGET = FrameForge
TEMPLATE = app

SOURCES += \
    consistencydialog.cpp \
    main.cpp \
    mainwindow.cpp \
    messagedialog.cpp \
    messagelayoutwidget.cpp \
    messagepropertieswidget.cpp \
    messagetablemodel.cpp \
    navigationfiltermodel.cpp \
    navigationmodel.cpp \
    networkdialog.cpp \
    networkpropertieswidget.cpp \
    nodedialog.cpp \
    nodepropertieswidget.cpp \
    nodetablemodel.cpp \
    signaldialog.cpp \
    signalpropertieswidget.cpp \
    signaltablemodel.cpp \
    valuetabledialog.cpp \
    valuetablepropertieswidget.cpp \
    valuetabletablemodel.cpp

HEADERS += \
    consistencydialog.h \
    mainwindow.h \
    messagedialog.h \
    messagelayoutwidget.h \
    messagepropertieswidget.h \
    messagetablemodel.h \
    navigationfiltermodel.h \
    navigationmodel.h \
    networkdialog.h \
    networkpropertieswidget.h \
    nodedialog.h \
    nodepropertieswidget.h \
    nodetablemodel.h \
    propertytable.h \
    signaldialog.h \
    signalpropertieswidget.h \
    signaltablemodel.h \
    valuetabledialog.h \
    valuetablepropertieswidget.h \
    valuetabletablemodel.h

RESOURCES += \
    resources.qrc

# The data, the Database and .ffxml reading/writing come from the FrameForgeCore library.
include(../FrameForgeCore/FrameForgeCore.pri)
