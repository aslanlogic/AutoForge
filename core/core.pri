# Include this from a program's .pro file to use the core library:
#     include(../core/core.pri)
# It makes the core headers findable and links the library.

INCLUDEPATH += $$PWD
DEPENDPATH += $$PWD

# Where qmake puts the library depends on the platform: Windows builds have
# separate debug/ and release/ folders.
CORE_BUILD_DIR = $$OUT_PWD/../core
win32:CONFIG(release, debug|release): CORE_BUILD_DIR = $$CORE_BUILD_DIR/release
else:win32:CONFIG(debug, debug|release): CORE_BUILD_DIR = $$CORE_BUILD_DIR/debug

LIBS += -L$$CORE_BUILD_DIR -lcore

# Relink the program when the library changed.
win32-msvc*: PRE_TARGETDEPS += $$CORE_BUILD_DIR/core.lib
else: PRE_TARGETDEPS += $$CORE_BUILD_DIR/libcore.a
