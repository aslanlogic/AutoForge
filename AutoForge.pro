# AutoForge: builds the shared libraries and the programs that use them.
#
#   ffcore/      message database, .ffxml files, consistency checks (QtCore only)
#   FrameForge/  the database editor (GUI)
#   usccore/     diagnostic description, .uscxml files (QtCore only)
#   UDScribe/    the diagnostic description editor (GUI)
#
# The traffic analyser DiagProbe will be added here later.

TEMPLATE = subdirs

SUBDIRS = \
    ffcore \
    FrameForge \
    usccore \
    UDScribe

# Build each library before the program that links against it.
FrameForge.depends = ffcore
UDScribe.depends = usccore
