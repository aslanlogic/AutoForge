# AutoForge: builds the shared library and the programs that use it.
#
#   ffcore/      message database, .ffxml files, consistency checks (QtCore only)
#   FrameForge/  the database editor (GUI)
#
# The traffic analyser DiagProbe will be added here as a third subdirectory.

TEMPLATE = subdirs

SUBDIRS = \
    ffcore \
    FrameForge

# Build ffcore first: FrameForge links against it.
FrameForge.depends = ffcore
