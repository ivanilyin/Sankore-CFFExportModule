#-------------------------------------------------
#
# Project created by QtCreator 2012-02-14T13:30:14
#
#-------------------------------------------------

win32: SUB_DIR = win
macx: SUB_DIR = macx
linux-g++: SUB_DIR = linux
linux-g++-32: SUB_DIR = linux
linux-g++-64: SUB_DIR = linux

QUAZIP_DIR   = "$$PWD/../quazip"

INCLUDEPATH += src\
            "$$QUAZIP_DIR/quazip-0.3" \
            "$$PWD/../zlib/1.2.3/include"

LIBS        += "-L$$QUAZIP_DIR/lib/$$SUB_DIR" "-lquazip"


QT       += xml xmlpatterns core
QT       -= gui

TARGET = CFF_Adaptor
TEMPLATE = lib

DEFINES += CFF_ADAPTOR_LIBRARY

SOURCES += \
    src/UBCFFAdaptor.cpp

HEADERS +=\
    src/UBCFFAdaptor.h \
    src/UBCFFAdaptor_global.h \
    src/UBGlobals.h

unix:!symbian {
    maemo5 {
        target.path = /opt/usr/lib
    } else {
        target.path = /usr/lib
    }
    INSTALLS += target
}

OBJECTS_DIR = $$PWD/objects
MOC_DIR = $$PWD/moc
DESTDIR = $$PWD/lib/$$SUB_DIR

DEFINES += NO_THIRD_PARTY_WARNINGS
