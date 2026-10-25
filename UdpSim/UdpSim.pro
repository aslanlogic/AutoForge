# UdpSim: a simulated BodyECU on UDP, for trying DiagProbe without hardware.
# Plain C++17 with the standard library only, plus the POSIX socket API for UDP
# (see udpsocket.cpp); no Qt.
TEMPLATE = app
CONFIG += console c++17
CONFIG -= qt app_bundle

TARGET = UdpSim

SOURCES += \
    bodyecu.cpp \
    main.cpp \
    udpsocket.cpp \
    vehiclebus.cpp

HEADERS += \
    bodyecu.h \
    bytes.h \
    udpsocket.h \
    vehiclebus.h
