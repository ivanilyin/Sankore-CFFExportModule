#-------------------------------------------------
#
# Project created by QtCreator 2012-02-14T17:39:08
#
#-------------------------------------------------

QT       += core
QT       -= gui

TARGET = launcherApp
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

INCLUDEPATH += "../UBCFFAdaptor/src"


win32:        LIBS += "-L../UBCFFAdaptor/lib/win32" "-lCFF_Adaptor"
linux-g++-32: LIBS += "-L../UBCFFAdaptor/lib/linux" "-lCFF_Adaptor"
linux-g++-64: LIBS += "-L../UBCFFAdaptor/lib/linux" "-lCFF_Adaptor"
linux-g++:    LIBS += "-L../UBCFFAdaptor/lib/linux" "-lCFF_Adaptor"
macx:         LIBS += "-L../UBCFFAdaptor/lib/mac"   "-lCFF_Adaptor"

SOURCES += main.cpp
