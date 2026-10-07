TARGET = harbour-admirality

CONFIG += sailfishapp
QT += gui network dbus
PKGCONFIG += qt5embedwidget

VERSION = 0.2.3
DEFINES += APP_VERSION=\\\"$$VERSION\\\"

SOURCES += \
    src/harbour-admirality.cpp \
    src/wilmabridge.cpp

HEADERS += \
    src/wilmabridge.h

DISTFILES += \
    rpm/harbour-admirality.spec \
    harbour-admirality.desktop \
    harbour-admirality.service \
    qml/harbour-admirality.qml \
    qml/cover/CoverPage.qml \
    qml/cover/icon-cover-settings.png \
    qml/pages/*.qml \
    eventsview/*.qml \
    eventsview/*.json \
    eventsview/*.ts \
    sailjail/AdmiralityDBus.permission \
    daemon/harbour-admiralityd.pro

icon86.files = icons/86x86/harbour-admirality.png
icon86.path = /usr/share/icons/hicolor/86x86/apps
icon108.files = icons/108x108/harbour-admirality.png
icon108.path = /usr/share/icons/hicolor/108x108/apps
icon128.files = icons/128x128/harbour-admirality.png
icon128.path = /usr/share/icons/hicolor/128x128/apps
icon172.files = icons/172x172/harbour-admirality.png
icon172.path = /usr/share/icons/hicolor/172x172/apps

eventsWidgetQml.files = eventsview/ScheduleWidget.qml \
                        eventsview/ScheduleContent.qml \
                        eventsview/InfoWidget.qml
eventsWidgetQml.path = /usr/share/harbour-admirality/eventsview

eventsWidgetJson.files = eventsview/harbour-admirality.json
eventsWidgetJson.path = /usr/share/lipstick/eventswidgets

# Settings → Events view shows the description only when qtTrId() resolves
# description_id. The catalog is /usr/share/translations/harbour-admirality_eng_en.qm.
EVENT_WIDGET_TS = $$PWD/eventsview/harbour-admirality.ts
EVENT_WIDGET_QM = $$OUT_PWD/harbour-admirality_eng_en.qm
eventswidget_qm.target = $$EVENT_WIDGET_QM
eventswidget_qm.depends = $$EVENT_WIDGET_TS
eventswidget_qm.commands = $$[QT_INSTALL_BINS]/lrelease -idbased $$EVENT_WIDGET_TS -qm $$EVENT_WIDGET_QM
QMAKE_EXTRA_TARGETS += eventswidget_qm
PRE_TARGETDEPS += $$EVENT_WIDGET_QM

eventsWidgetQm.files = $$EVENT_WIDGET_QM
eventsWidgetQm.path = /usr/share/translations
eventsWidgetQm.CONFIG += no_check_exist

sailjailPermission.files = sailjail/AdmiralityDBus.permission
sailjailPermission.path = /etc/sailjail/permissions

INSTALLS += icon86 icon108 icon128 icon172 eventsWidgetQml eventsWidgetJson eventsWidgetQm sailjailPermission
