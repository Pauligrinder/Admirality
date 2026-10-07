TARGET = harbour-admiralityd
TEMPLATE = app
QT = core network dbus
CONFIG += c++11 console link_pkgconfig
CONFIG -= app_bundle

VERSION = 0.2.2
DEFINES += APP_VERSION=\\\"$$VERSION\\\"

INCLUDEPATH += ../src

SOURCES += \
    ../src/daemonmain.cpp \
    ../src/wilmaservice.cpp \
    ../src/wilmaclient.cpp

HEADERS += \
    ../src/wilmaservice.h \
    ../src/wilmaclient.h

RESOURCES += ../admirality.qrc

target.path = /usr/bin
systemd.files = ../harbour-admirality.service
systemd.path = /usr/lib/systemd/user
INSTALLS += target systemd
