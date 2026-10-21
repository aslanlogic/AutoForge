# The shared part of AutoForge: the data structures, the Database, reading
# and writing .ffxml files and the consistency checks.
# QtCore only, so it never depends on GUI code.

QT = core

CONFIG += c++17 staticlib

TARGET = FrameForgeCore
TEMPLATE = lib

SOURCES += \
    consistencychecker.cpp \
    database.cpp \
    xmlstorage.cpp

HEADERS += \
    consistencychecker.h \
    database.h \
    message.h \
    network.h \
    node.h \
    signal.h \
    valuetable.h \
    xmlstorage.h
