import QtQuick 2.6
import Sailfish.Silica 1.0

Item {
    id: page
    objectName: "NotesPage"
    anchors.fill: parent

    property var days: {
        var groups = []
        var map = {}
        var all = wilmaClient.lessonNotes || []
        for (var i = 0; i < all.length; ++i) {
            var note = all[i]
            var key = note.date || note.dateLabel || String(i)
            if (!map[key]) {
                map[key] = {
                    date: key,
                    dateLabel: note.dateLabel || note.date || qsTr("Day"),
                    items: []
                }
                groups.push(map[key])
            }
            map[key].items.push(note)
        }
        return groups
    }

    function detailLine(item) {
        var parts = []
        if (item.start && item.end)
            parts.push(item.start + "–" + item.end)
        else if (item.start)
            parts.push(item.start)
        if (item.subject)
            parts.push(item.subject)
        if (item.teacher)
            parts.push(item.teacher)
        return parts.join(" · ")
    }

    Component.onCompleted: wilmaClient.acknowledgeNotes()

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        VerticalScrollDecorator {}

        PullDownMenu {
            busy: wilmaClient.refreshing
            MenuItem {
                text: qsTr("Settings")
                onClicked: pageStack.push(Qt.resolvedUrl("SettingsPage.qml"))
            }
            MenuItem {
                text: qsTr("Open Wilma site")
                onClicked: pageStack.push(Qt.resolvedUrl("WilmaPage.qml"))
            }
            MenuItem {
                visible: wilmaClient.roles.length > 1
                text: qsTr("Change user")
                onClicked: pageStack.push(Qt.resolvedUrl("RolePickerPage.qml"))
            }
            MenuItem {
                text: qsTr("Refresh")
                onClicked: wilmaClient.refreshHome()
            }
        }

        Column {
            id: column
            width: parent.width

            PageHeader {
                title: qsTr("Lesson notes")
                description: wilmaClient.roleName.length > 0
                             ? wilmaClient.roleName
                             : wilmaClient.displayName
            }

            Repeater {
                model: page.days
                delegate: Column {
                    width: column.width

                    SectionHeader {
                        text: modelData.dateLabel
                    }

                    Repeater {
                        model: modelData.items
                        delegate: BackgroundItem {
                            width: column.width
                            height: noteCol.height + Theme.paddingMedium * 2
                            enabled: false

                            Rectangle {
                                id: noteDot
                                anchors {
                                    left: parent.left
                                    leftMargin: Theme.horizontalPageMargin
                                    verticalCenter: parent.verticalCenter
                                }
                                width: Theme.paddingMedium
                                height: width
                                radius: width / 2
                                color: modelData.dotColor ? modelData.dotColor : "#9e9e9e"
                            }

                            Column {
                                id: noteCol
                                anchors {
                                    left: noteDot.right
                                    right: parent.right
                                    verticalCenter: parent.verticalCenter
                                    leftMargin: Theme.paddingMedium
                                    rightMargin: Theme.horizontalPageMargin
                                }

                                Label {
                                    width: parent.width
                                    wrapMode: Text.Wrap
                                    text: modelData.typeLabel || qsTr("Lesson note")
                                    font.bold: modelData.needsAction
                                    color: modelData.needsAction ? Theme.highlightColor
                                                                 : Theme.primaryColor
                                }

                                Label {
                                    width: parent.width
                                    wrapMode: Text.Wrap
                                    visible: page.detailLine(modelData).length > 0
                                    color: Theme.secondaryColor
                                    font.pixelSize: Theme.fontSizeExtraSmall
                                    text: page.detailLine(modelData)
                                }

                                Label {
                                    width: parent.width
                                    wrapMode: Text.Wrap
                                    visible: (modelData.comment || "").length > 0
                                    color: Theme.secondaryHighlightColor
                                    font.pixelSize: Theme.fontSizeExtraSmall
                                    text: modelData.comment
                                }
                            }
                        }
                    }
                }
            }

            Label {
                visible: wilmaClient.lessonNotes.length === 0
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeSmall
                text: wilmaClient.refreshing
                      ? qsTr("Loading lesson notes…")
                      : qsTr("No lesson notes")
            }
        }
    }
}
