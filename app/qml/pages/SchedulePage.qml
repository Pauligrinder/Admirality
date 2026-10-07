import QtQuick 2.6
import Sailfish.Silica 1.0

Item {
    id: page
    objectName: "SchedulePage"
    anchors.fill: parent

    property string highlightDate: appWindow.pendingScheduleDate || ""

    property var days: {
        var groups = []
        var map = {}
        var all = wilmaClient.schedule || []
        for (var i = 0; i < all.length; ++i) {
            var lesson = all[i]
            var key = lesson.date || lesson.dateLabel || String(i)
            if (!map[key]) {
                map[key] = {
                    date: key,
                    dateLabel: lesson.dateLabel || lesson.date || qsTr("Day"),
                    items: []
                }
                groups.push(map[key])
            }
            map[key].items.push(lesson)
        }
        return groups
    }

    function scrollToHighlight() {
        if (!page.highlightDate.length)
            return
        for (var i = 0; i < dayRepeater.count; ++i) {
            var item = dayRepeater.itemAt(i)
            if (!item || item.dayKey !== page.highlightDate)
                continue
            var p = item.mapToItem(flickable.contentItem, 0, 0)
            flickable.contentY = Math.max(0, p.y - Theme.paddingLarge)
            return
        }
    }

    onHighlightDateChanged: scrollTimer.restart()
    onDaysChanged: if (page.highlightDate.length) scrollTimer.restart()

    Timer {
        id: scrollTimer
        interval: 80
        repeat: false
        onTriggered: page.scrollToHighlight()
    }

    Component.onCompleted: if (page.highlightDate.length) scrollTimer.start()

    SilicaFlickable {
        id: flickable
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
                title: qsTr("Schedule")
                description: wilmaClient.roleName.length > 0
                             ? wilmaClient.roleName
                             : wilmaClient.displayName
            }

            Repeater {
                id: dayRepeater
                model: page.days
                delegate: Column {
                    property string dayKey: modelData.date
                    width: column.width

                    SectionHeader {
                        text: modelData.dateLabel
                        color: page.highlightDate === modelData.date
                               ? Theme.highlightColor
                               : Theme.secondaryHighlightColor
                    }

                    Repeater {
                        model: modelData.items
                        delegate: BackgroundItem {
                            width: column.width
                            height: lessonCol.height + Theme.paddingMedium * 2
                            enabled: false

                            Column {
                                id: lessonCol
                                anchors {
                                    left: parent.left
                                    right: parent.right
                                    verticalCenter: parent.verticalCenter
                                    leftMargin: Theme.horizontalPageMargin
                                    rightMargin: Theme.horizontalPageMargin
                                }

                                Label {
                                    width: parent.width
                                    wrapMode: Text.Wrap
                                    text: modelData.subject
                                    color: Theme.primaryColor
                                }

                                Label {
                                    width: parent.width
                                    wrapMode: Text.Wrap
                                    color: Theme.secondaryColor
                                    font.pixelSize: Theme.fontSizeExtraSmall
                                    text: {
                                        var time = modelData.start && modelData.end
                                                  ? modelData.start + "–" + modelData.end
                                                  : (modelData.start || "")
                                        var extra = []
                                        if (modelData.teacher)
                                            extra.push(modelData.teacher)
                                        if (modelData.subjectCode)
                                            extra.push(modelData.subjectCode)
                                        return extra.length ? time + " · " + extra.join(" · ") : time
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Label {
                visible: wilmaClient.schedule.length === 0
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeSmall
                text: wilmaClient.refreshing ? qsTr("Loading…") : qsTr("No schedule")
            }
        }
    }
}
