QT += core gui widgets

CONFIG += c++17

TARGET = FrameForge
TEMPLATE = app

SOURCES += \
    database.cpp \
    main.cpp \
    mainwindow.cpp \
    navigationfiltermodel.cpp \
    navigationmodel.cpp \
    signaldialog.cpp \
    signalpropertieswidget.cpp \
    signaltablemodel.cpp \
    xmlstorage.cpp

HEADERS += \
    database.h \
    mainwindow.h \
    navigationfiltermodel.h \
    navigationmodel.h \
    signal.h \
    signaldialog.h \
    signalpropertieswidget.h \
    signaltablemodel.h \
    xmlstorage.h

RESOURCES += \
    resources.qrc
