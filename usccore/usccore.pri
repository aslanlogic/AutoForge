# Include this from a program's .pro file to use the usccore library:
#     include(../usccore/usccore.pri)
# It links the library and makes the headers findable as
#     #include "usccore/<header>.h"
# so headers with the same name in other libraries cannot be mixed up.

INCLUDEPATH += $$PWD/..
DEPENDPATH += $$PWD

# Where qmake puts the library depends on the platform: Windows builds have
# separate debug/ and release/ folders.
CORE_BUILD_DIR = $$OUT_PWD/../usccore
win32:CONFIG(release, debug|release): CORE_BUILD_DIR = $$CORE_BUILD_DIR/release
else:win32:CONFIG(debug, debug|release): CORE_BUILD_DIR = $$CORE_BUILD_DIR/debug

LIBS += -L$$CORE_BUILD_DIR -lusccore

# Relink the program when the library changed.
win32-msvc*: PRE_TARGETDEPS += $$CORE_BUILD_DIR/usccore.lib
else: PRE_TARGETDEPS += $$CORE_BUILD_DIR/libusccore.a
