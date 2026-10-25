# SeedKey: an example key algorithm plugin for DiagProbe (see DiagProbe/seedkey.h).
# Plain C++, no Qt. Select the built libSeedKey.so with File > Select Key Algorithm.
TEMPLATE = lib
CONFIG += plugin c++17     # plugin: just libSeedKey.so, no version numbers
CONFIG -= qt

TARGET = SeedKey

INCLUDEPATH += $$PWD/../../DiagProbe

SOURCES += seedkey.cpp
HEADERS += ../../DiagProbe/seedkey.h
