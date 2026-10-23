# The shared part of UDScribe: its data structures and reading and writing
# .uscxml files. Everything is in namespace usc.
# QtCore only, so it never depends on GUI code.

QT = core

CONFIG += c++17 staticlib

TARGET = usccore
TEMPLATE = lib

SOURCES += \
    database.cpp \
    xmlstorage.cpp

HEADERS += \
    database.h \
    securitylevel.h \
    xmlstorage.h
