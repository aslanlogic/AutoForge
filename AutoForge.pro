# AutoForge: builds the shared libraries and the programs that use them.
#
#   ffcore/      message database, .ffxml files, consistency checks (QtCore only)
#   FrameForge/  the database editor (GUI)
#   usccore/     diagnostic description, .uscxml files (QtCore only)
#   UDScribe/    the diagnostic description editor (GUI)
#   DiagProbe/   the diagnostic tester (GUI); uses both libraries

TEMPLATE = subdirs

SUBDIRS = \
    ffcore \
    FrameForge \
    usccore \
    UDScribe \
    DiagProbe

# Build each library before the program that links against it.
FrameForge.depends = ffcore
UDScribe.depends = usccore
DiagProbe.depends = ffcore usccore
