import QtQuick 2.6
import Sailfish.Silica 1.0

// Lipstick loads this file with a bare Loader that only sets width. Match the
// stock weather/calendar loaders: always report a height, and load the real
// UI as a child so a content error cannot collapse the slot to zero.
Loader {
    id: root

    width: parent ? parent.width : Screen.width
    implicitWidth: width
    height: Math.max(Theme.itemSizeMedium,
                     (status === Loader.Ready && item) ? item.implicitHeight
                                                       : Theme.itemSizeMedium)
    implicitHeight: height

    property bool active: visible && eventsViewVisible

    source: Qt.resolvedUrl("ScheduleContent.qml")

    function refresh() {
        if (item && typeof item.refresh === "function")
            item.refresh()
    }
    function reload() { refresh() }
    function save() {
        if (item && typeof item.save === "function")
            item.save()
    }

    onActiveChanged: {
        if (item)
            item.active = root.active
    }
    onStatusChanged: {
        if (status === Loader.Ready && item) {
            item.active = root.active
            if (root.active)
                root.refresh()
        }
    }
}
