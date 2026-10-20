QT += core gui widgets

CONFIG += c++17

TARGET = FrameForge
TEMPLATE = app

SOURCES += \
    consistencychecker.cpp \
    consistencydialog.cpp \
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
    valuetabledialog.cpp \
    valuetablepropertieswidget.cpp \
    valuetabletablemodel.cpp \
    xmlstorage.cpp

HEADERS += \
    consistencychecker.h \
    consistencydialog.h \
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
    valuetable.h \
    valuetabledialog.h \
    valuetablepropertieswidget.h \
    valuetabletablemodel.h \
    xmlstorage.h

RESOURCES += \
    resources.qrc
