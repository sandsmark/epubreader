#-------------------------------------------------
#
# Project created by QtCreator 2016-07-13T16:31:39
#
#-------------------------------------------------

QT       += core gui xml
CONFIG   += c++11

QT       += KArchive

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = epubreader
TEMPLATE = app


SOURCES += main.cpp\
        widget.cpp \
    epubcontainer.cpp

HEADERS  += widget.h \
    epubcontainer.h
