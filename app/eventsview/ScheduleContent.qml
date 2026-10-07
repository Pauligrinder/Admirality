import QtQuick 2.6
import Sailfish.Silica 1.0

// Compatibility shim — lipstick loads ScheduleWidget.qml.
Item {
    width: parent ? parent.width : Screen.width
    height: Theme.itemSizeMedium
    implicitHeight: height
}
