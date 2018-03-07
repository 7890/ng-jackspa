# qmake project file
QMAKE_MAKEFILE = Makefile.qmake
TEMPLATE = app
TARGET = qjackspa
CONFIG += debug link_pkgconfig
PKGCONFIG += glib-2.0
DEPENDPATH += .
INCLUDEPATH += .
DEFINES += G_DISABLE_DEPRECATED
LIBS += -ljack -ldl -lm

# Input
HEADERS += control.h jackspa.h qjackspa.h
SOURCES += jackspa.c
SOURCES += control.c
SOURCES += qjackspa.cpp
