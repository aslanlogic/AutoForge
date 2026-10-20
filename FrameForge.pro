QT += core gui widgets

CONFIG += c++17

TARGET = FrameForge
TEMPLATE = app

SOURCES += \
    database.cpp \
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
    xmlstorage.cpp

HEADERS += \
    database.h \
    message.h \
    mainwindow.h \
    messagedialog.h \
    messagelayoutwidget.h \
    messagepropertieswidget.h \
    messagetablemodel.h \
    navigationfiltermodel.h \
    navigationmodel.h \
    network.h \
    networkdialog.h \
    networkpropertieswidget.h \
    node.h \
    nodedialog.h \
    nodepropertieswidget.h \
    nodetablemodel.h \
    propertytable.h \
    signal.h \
    signaldialog.h \
    signalpropertieswidget.h \
    signaltablemodel.h \
    xmlstorage.h

RESOURCES += \
    resources.qrc
