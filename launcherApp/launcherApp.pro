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
LIBS +=      "-L../UBCFFAdaptor/lib/linux" "-lCFF_Adaptor"

SOURCES += main.cpp
