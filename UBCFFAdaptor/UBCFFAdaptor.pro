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

INCLUDEPATH += src

QT       += xml xmlpatterns core
QT       -= gui

TARGET = CFF_Adaptor
TEMPLATE = lib

DEFINES += CFF_ADAPTOR_LIBRARY

SOURCES += \
    src/UBCFFAdaptor.cpp

HEADERS +=\
    src/UBCFFAdaptor.h \
    src/UBCFFAdaptor_global.h

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
