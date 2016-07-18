#-------------------------------------------------
#
# Project created by QtCreator 2016-07-13T16:31:39
#
#-------------------------------------------------

QT       += core gui xml widgets svg

## For debugging CSS
# QT += gui-private
# DEFINES += DEBUG_CSS

CONFIG   += c++11

QT       += KArchive

TARGET = epubreader
TEMPLATE = app


SOURCES += main.cpp\
        widget.cpp \
    epubcontainer.cpp \
    epubdocument.cpp

HEADERS  += widget.h \
    epubcontainer.h \
    epubdocument.h
