# AutoForge: builds the shared library and the programs that use it.
#
#   core/        message database, .ffxml files, consistency checks (QtCore only)
#   FrameForge/  the database editor (GUI)
#
# The traffic analyser DiagProbe will be added here as a third subdirectory.

TEMPLATE = subdirs

SUBDIRS = \
    core \
    FrameForge

# Build core first: FrameForge links against it.
FrameForge.depends = core
