QT += core gui network sql

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    myocr.cpp \
    mythread.cpp

HEADERS += \
    mainwindow.h \
    myocr.h \
    mythread.h

FORMS += \
    mainwindow.ui

DISTFILES += \
    Py_Module.py

win32: LIBS += -L$$PWD/hunhe/libs/ -lpython310

INCLUDEPATH += $$PWD/hunhe/include
DEPENDPATH += $$PWD/hunhe/include
