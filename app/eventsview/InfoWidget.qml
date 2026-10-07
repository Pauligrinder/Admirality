import QtQuick 2.6
import Sailfish.Silica 1.0
import Nemo.DBus 2.0

Item {
    id: root

    width: parent ? parent.width : Screen.width
    implicitWidth: width
    implicitHeight: column.height
    height: implicitHeight

    property bool active: visible && eventsViewVisible
    property bool appRunning: false
    property int messages: 0
    property int notes: 0
    property int news: 0
    property int grades: 0
    property int homework: 0
    property int exams: 0

    readonly property var counts: [
        { "icon": "image://theme/icon-m-mail", "count": root.messages },
        { "icon": "image://theme/icon-m-document", "count": root.notes },
        { "icon": "image://theme/icon-m-events", "count": root.news },
        { "icon": "image://theme/icon-m-favorite", "count": root.grades },
        { "icon": "image://theme/icon-m-edit", "count": root.homework },
        { "icon": "image://theme/icon-m-calendar", "count": root.exams }
    ]

    function applyPayload(payload) {
        root.appRunning = true
        if (!payload)
            return
        try {
            var state = JSON.parse(payload)
            root.messages = Number(state.unreadCount) || 0
            root.notes = Number(state.freshNoteCount) || 0
            root.news = Number(state.freshNewsCount) || 0
            root.grades = Number(state.freshGradeCount) || 0
            root.homework = Number(state.freshHomeworkCount) || 0
            root.exams = Number(state.freshExamCount) || 0
        } catch (e) {
            root.appRunning = false
        }
    }

    function fetchState() {
        if (!root.active)
            return
        wilma.call("GetState", [],
                   function(result) { root.applyPayload(result) },
                   function() { root.appRunning = false })
    }

    function refresh() {
        if (!root.active)
            return
        root.fetchState()
    }

    function reload() { refresh() }
    function save() {}

    Component.onCompleted: if (active) refresh()
    onActiveChanged: if (active) refresh()

    Timer {
        interval: 30000
        repeat: true
        running: root.active
        onTriggered: root.fetchState()
    }

    DBusInterface {
        id: wilma
        service: "org.admirality.harbour-admirality"
        path: "/wilma"
        iface: "org.admirality.Wilma"
        signalsEnabled: root.active
        // Nemo.DBus maps StateChanged to a lowercase-initial handler, same as Helmsman.
        function stateChanged() {
            root.fetchState()
        }
    }

    Column {
        id: column
        width: parent.width

        Item {
            x: Theme.horizontalPageMargin
            width: parent.width - 2 * x
            height: row.height + Theme.paddingLarge * 2 + Theme.paddingMedium

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter
                width: Math.min(parent.width, row.width + Theme.paddingLarge * 2)
                height: row.height + Theme.paddingLarge * 2
                radius: Theme.paddingMedium
                color: "#03A9F4"
                opacity: root.appRunning ? 0.32 : 0.16
            }

            Row {
                id: row
                anchors.centerIn: parent
                spacing: Theme.paddingLarge

                Repeater {
                    model: root.counts
                    delegate: Item {
                        width: Math.max(icon.width, badge.implicitWidth)
                        height: icon.height

                        Image {
                            id: icon
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: Theme.iconSizeMedium
                            height: Theme.iconSizeMedium
                            sourceSize.width: width
                            sourceSize.height: height
                            source: modelData.icon
                            opacity: modelData.count > 0 ? 1 : 0.4
                        }

                        Label {
                            id: badge
                            anchors.right: icon.right
                            anchors.bottom: icon.bottom
                            anchors.rightMargin: -Theme.paddingSmall
                            anchors.bottomMargin: -Theme.paddingSmall
                            font.pixelSize: Theme.fontSizeSmall
                            font.bold: modelData.count > 0
                            color: modelData.count > 0 ? Theme.highlightColor : Theme.secondaryColor
                            text: modelData.count
                        }
                    }
                }
            }
        }
    }
}
