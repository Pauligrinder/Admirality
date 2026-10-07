import QtQuick 2.6
import Sailfish.Silica 1.0
import Nemo.DBus 2.0
import org.nemomobile.lipstick 0.1

Item {
    id: root

    width: parent ? parent.width : Screen.width
    implicitWidth: width
    implicitHeight: Math.max(column.height, Theme.itemSizeMedium)
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
        { "icon": "image://theme/icon-m-mail", "count": root.messages, "view": "messages" },
        { "icon": "image://theme/icon-m-document", "count": root.notes, "view": "notes" },
        { "icon": "image://theme/icon-m-events", "count": root.news, "view": "news" },
        { "icon": "image://theme/icon-m-favorite", "count": root.grades, "view": "grades" },
        { "icon": "image://theme/icon-m-edit", "count": root.homework, "view": "homework" },
        { "icon": "image://theme/icon-m-calendar", "count": root.exams, "view": "exams" }
    ]

    function findApp(model, depth) {
        if (!model || depth > 4 || typeof model.get !== "function")
            return null
        var n = model.count || 0
        for (var i = 0; i < n; ++i) {
            var item = model.get(i)
            if (!item)
                continue
            var path = String(item.filePath || item.exec || "")
            if (path.indexOf("harbour-admirality") >= 0)
                return item
            if (String(item.title || "") === "Admirality")
                return item
            if (item.type === LauncherModel.Folder) {
                var nested = root.findApp(item, depth + 1)
                if (!nested && item.model)
                    nested = root.findApp(item.model, depth + 1)
                if (nested)
                    return nested
            }
        }
        return null
    }

    function launchApp() {
        // Runs inside lipstick — launchApplication raises or starts immediately.
        var item = root.findApp(launcherModel, 0)
        if (item) {
            item.launchApplication()
            return true
        }
        return false
    }

    function openCount(view) {
        // Tell the daemon which page to open, then raise the UI from lipstick
        // so the tap feels instant (invoker alone is slow on a cold start).
        wilma.call("OpenView", [view])
        root.launchApp()
    }

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

    LauncherModel {
        id: launcherModel
    }

    DBusInterface {
        id: wilma
        service: "org.admirality.harbour-admirality"
        path: "/wilma"
        iface: "org.admirality.Wilma"
        signalsEnabled: root.active
        function stateChanged() {
            root.fetchState()
        }
    }

    Column {
        id: column
        width: parent.width
        spacing: Theme.paddingSmall

        Label {
            x: Theme.horizontalPageMargin
            width: parent.width - 2 * x
            text: "Wilma"
            color: Theme.highlightColor
            font.pixelSize: Theme.fontSizeMedium
            font.family: Theme.fontFamilyHeading
            truncationMode: TruncationMode.Fade
        }

        Item {
            x: Theme.horizontalPageMargin
            width: parent.width - 2 * x
            height: Theme.iconSizeMedium + Theme.paddingMedium * 2

            Rectangle {
                anchors.fill: parent
                radius: Theme.paddingSmall
                color: "#03A9F4"
                opacity: root.appRunning ? 0.32 : 0.16
            }

            Row {
                id: row
                anchors.centerIn: parent
                width: parent.width - Theme.paddingSmall * 2
                height: Theme.iconSizeMedium
                spacing: 0

                Repeater {
                    model: root.counts
                    delegate: BackgroundItem {
                        width: Math.floor(row.width / root.counts.length)
                        height: row.height
                        onClicked: root.openCount(modelData.view)

                        Image {
                            id: icon
                            anchors.centerIn: parent
                            width: Theme.iconSizeSmall
                            height: Theme.iconSizeSmall
                            sourceSize.width: width
                            sourceSize.height: height
                            source: modelData.icon
                            opacity: parent.highlighted ? 0.45
                                     : (modelData.count > 0 ? 1 : 0.45)
                        }

                        Label {
                            anchors.right: icon.right
                            anchors.bottom: icon.bottom
                            anchors.rightMargin: -Theme.paddingSmall
                            anchors.bottomMargin: -Theme.paddingSmall
                            font.pixelSize: Theme.fontSizeExtraSmall
                            font.bold: modelData.count > 0
                            color: parent.highlighted ? Theme.highlightColor
                                   : (modelData.count > 0 ? Theme.highlightColor
                                                          : Theme.secondaryColor)
                            text: modelData.count
                        }
                    }
                }
            }
        }
    }
}
