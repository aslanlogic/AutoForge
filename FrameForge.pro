QT += core gui widgets

CONFIG += c++17

TARGET = FrameForge
TEMPLATE = app

SOURCES += \
    database.cpp \
    main.cpp \
    mainwindow.cpp \
    navigationmodel.cpp \
    signaldialog.cpp

HEADERS += \
    database.h \
    mainwindow.h \
    navigationmodel.h \
    signal.h \
    signaldialog.h
