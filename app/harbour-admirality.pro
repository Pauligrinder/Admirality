TARGET = harbour-admirality

CONFIG += sailfishapp
QT += gui network
PKGCONFIG += qt5embedwidget

VERSION = 0.1.0
DEFINES += APP_VERSION=\\\"$$VERSION\\\"

SOURCES += \
    src/harbour-admirality.cpp \
    src/wilmaclient.cpp

HEADERS += \
    src/wilmaclient.h

RESOURCES += admirality.qrc

DISTFILES += \
    rpm/harbour-admirality.spec \
    harbour-admirality.desktop \
    qml/harbour-admirality.qml \
    qml/cover/CoverPage.qml \
    qml/cover/icon-cover-settings.png \
    qml/pages/*.qml

icon86.files = icons/86x86/harbour-admirality.png
icon86.path = /usr/share/icons/hicolor/86x86/apps
icon108.files = icons/108x108/harbour-admirality.png
icon108.path = /usr/share/icons/hicolor/108x108/apps
icon128.files = icons/128x128/harbour-admirality.png
icon128.path = /usr/share/icons/hicolor/128x128/apps
icon172.files = icons/172x172/harbour-admirality.png
icon172.path = /usr/share/icons/hicolor/172x172/apps

INSTALLS += icon86 icon108 icon128 icon172
