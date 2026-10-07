import QtQuick 2.6
import Sailfish.Silica 1.0
import Nemo.DBus 2.0
import org.nemomobile.lipstick 0.1

Item {
    id: root

    // Same navy as the app cover.
    readonly property color coverBlue: "#0B365C"

    width: parent ? parent.width : Screen.width
    implicitWidth: width
    height: Math.max(column.height, Theme.itemSizeMedium)
    implicitHeight: height

    property bool active: visible && eventsViewVisible
    property bool appRunning: false
    property var desktopEntry: null
    property int messages: 0
    property int notes: 0
    property int news: 0
    property int grades: 0
    property int homework: 0
    property int exams: 0
    property string roleId: ""
    property string roleName: ""
    property string displayName: ""
    property var roles: []

    readonly property string personLabel: {
        var name = root.roleName.length ? root.roleName : root.displayName
        if (!name.length)
            return ""
        var parts = name.trim().split(/\s+/)
        return parts.length ? parts[0] : name
    }

    readonly property var counts: [
        { "icon": "image://theme/icon-m-mail?#FFFFFF", "count": root.messages, "view": "messages" },
        { "icon": "image://theme/icon-m-document?#FFFFFF", "count": root.notes, "view": "notes" },
        { "icon": "image://theme/icon-m-events?#FFFFFF", "count": root.news, "view": "news" },
        { "icon": "image://theme/icon-m-favorite?#FFFFFF", "count": root.grades, "view": "grades" },
        { "icon": "image://theme/icon-m-edit?#FFFFFF", "count": root.homework, "view": "homework" },
        { "icon": "image://theme/icon-m-date?#FFFFFF", "count": root.exams, "view": "exams" }
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

    function ensureDesktopEntry() {
        if (root.desktopEntry)
            return root.desktopEntry
        try {
            root.desktopEntry = Qt.createQmlObject(
                        'import org.nemomobile.lipstick 0.1; LauncherItem { }',
                        root, "AdmiralityDesktopEntry")
            root.desktopEntry.filePath = "/usr/share/applications/harbour-admirality.desktop"
        } catch (e) {
            root.desktopEntry = null
        }
        return root.desktopEntry
    }

    function launchApp() {
        var entry = root.ensureDesktopEntry()
        try {
            if (entry && typeof entry.launchApplication === "function") {
                entry.launchApplication()
                return true
            }
        } catch (e) { }
        var item = root.findApp(launcherModel, 0)
        if (item) {
            item.launchApplication()
            return true
        }
        return false
    }

    function openCount(view) {
        root.launchApp()
        wilma.call("OpenView", [view])
    }

    function cyclePerson() {
        var list = root.roles || []
        if (list.length < 2)
            return
        var idx = 0
        for (var i = 0; i < list.length; ++i) {
            if (String(list[i].id) === String(root.roleId)) {
                idx = i
                break
            }
        }
        var next = list[(idx + 1) % list.length]
        if (next && next.id)
            wilma.call("SelectRole", [String(next.id)])
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
            root.roleId = String(state.roleId || "")
            root.roleName = String(state.roleName || "")
            root.displayName = String(state.displayName || "")
            root.roles = state.roles || []
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

    Component.onCompleted: {
        root.ensureDesktopEntry()
        if (active)
            refresh()
    }
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

        Item {
            x: Theme.horizontalPageMargin
            width: parent.width - 2 * x
            // Tall enough for logo / person name above + icon row at the bottom.
            height: Theme.itemSizeLarge + Theme.iconSizeMedium + Theme.paddingMedium

            Rectangle {
                id: bar
                anchors.fill: parent
                radius: Theme.paddingSmall
                color: root.coverBlue
                opacity: root.appRunning ? 1.0 : 0.55
                clip: true

                // White Wilma mark — left, mostly above the buttons.
                Image {
                    id: logo
                    anchors.left: parent.left
                    anchors.leftMargin: Theme.paddingMedium
                    anchors.top: parent.top
                    anchors.topMargin: Theme.paddingSmall
                    width: parent.width * 0.42
                    height: parent.height * 0.72
                    fillMode: Image.PreserveAspectFit
                    horizontalAlignment: Image.AlignLeft
                    verticalAlignment: Image.AlignTop
                    smooth: true
                    asynchronous: true
                    opacity: 0.28
                    source: Qt.resolvedUrl("wilma-logo-white.png")
                    z: 0
                }

                // Person name — right side. Button cycles roles when there are several.
                Loader {
                    id: personLoader
                    z: 1
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Theme.paddingSmall
                    width: Math.min(parent.width * 0.48, Theme.itemSizeHuge * 1.4)
                    height: Theme.itemSizeSmall
                    active: root.personLabel.length > 0
                    sourceComponent: (root.roles && root.roles.length > 1)
                                     ? personButtonComponent
                                     : personLabelComponent
                }

                Component {
                    id: personLabelComponent
                    Item {
                        width: personLoader.width
                        height: personLoader.height
                        Label {
                            anchors.fill: parent
                            anchors.rightMargin: Theme.paddingSmall
                            horizontalAlignment: Text.AlignRight
                            verticalAlignment: Text.AlignVCenter
                            truncationMode: TruncationMode.Fade
                            color: "white"
                            font.pixelSize: Theme.fontSizeSmall
                            text: root.personLabel
                        }
                    }
                }

                Component {
                    id: personButtonComponent
                    BackgroundItem {
                        id: personButton
                        width: personLoader.width
                        height: personLoader.height
                        onClicked: root.cyclePerson()

                        Row {
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.rightMargin: Theme.paddingSmall
                            spacing: Theme.paddingSmall / 2

                            Label {
                                width: Math.min(implicitWidth,
                                                personLoader.width - Theme.iconSizeSmall
                                                - Theme.paddingMedium)
                                horizontalAlignment: Text.AlignRight
                                truncationMode: TruncationMode.Fade
                                color: "white"
                                font.pixelSize: Theme.fontSizeSmall
                                font.bold: true
                                text: root.personLabel
                                opacity: personButton.highlighted ? 0.6 : 1
                            }

                            Image {
                                anchors.verticalCenter: parent.verticalCenter
                                width: Theme.iconSizeSmall
                                height: Theme.iconSizeSmall
                                source: "image://theme/icon-m-users?#FFFFFF"
                                opacity: personButton.highlighted ? 0.5 : 0.85
                            }
                        }
                    }
                }

                Row {
                    id: row
                    z: 1
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: Theme.paddingSmall
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
                                opacity: parent.highlighted ? 0.55
                                         : (modelData.count > 0 ? 1.0 : 0.7)
                            }

                            Label {
                                anchors.right: icon.right
                                anchors.bottom: icon.bottom
                                anchors.rightMargin: -Theme.paddingSmall
                                anchors.bottomMargin: -Theme.paddingSmall
                                font.pixelSize: Theme.fontSizeExtraSmall
                                font.bold: modelData.count > 0
                                color: "white"
                                text: modelData.count
                            }
                        }
                    }
                }
            }
        }
    }
}
