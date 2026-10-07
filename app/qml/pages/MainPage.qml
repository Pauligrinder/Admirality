import QtQuick 2.6
import Sailfish.Silica 1.0

Page {
    id: page
    objectName: "MainPage"
    allowedOrientations: Orientation.All

    property int currentTab: 1

    function showTab(index) {
        if (index < 0 || index > 4)
            return
        page.currentTab = index
    }

    function showHomeSection(which) {
        page.currentTab = 0
        Qt.callLater(function() {
            if (tabLoader.item && typeof tabLoader.item.reveal === "function")
                tabLoader.item.reveal(which)
        })
    }

    function reloadWilma() {
        wilmaClient.refreshHome()
    }

    Component.onCompleted: {
        if (wilmaClient.loggedIn && wilmaClient.roleId.length > 0)
            wilmaClient.refreshHome()
    }

    Loader {
        id: tabLoader
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
            bottom: tabBar.top
        }
        clip: true
        source: {
            if (page.currentTab === 1)
                return Qt.resolvedUrl("MessagesPage.qml")
            if (page.currentTab === 2)
                return Qt.resolvedUrl("SchedulePage.qml")
            if (page.currentTab === 3)
                return Qt.resolvedUrl("NotesPage.qml")
            if (page.currentTab === 4)
                return Qt.resolvedUrl("NewsPage.qml")
            return Qt.resolvedUrl("HomePage.qml")
        }
    }

    Item {
        id: tabBar
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        height: Theme.itemSizeLarge + Theme.paddingSmall
        z: 8

        Rectangle {
            anchors.fill: parent
            color: Theme.highlightDimmerColor
            opacity: 0.92
        }

        Rectangle {
            anchors {
                left: parent.left
                right: parent.right
                top: parent.top
            }
            height: Math.max(1, Math.round(Theme.paddingSmall / 6))
            color: Theme.highlightColor
            opacity: 0.35
        }

        Row {
            anchors.fill: parent

            Repeater {
                model: [
                    {
                        title: qsTr("Home"),
                        icon: "image://theme/icon-m-home"
                    },
                    {
                        title: qsTr("Messages"),
                        icon: "image://theme/icon-m-mail"
                    },
                    {
                        title: qsTr("Schedule"),
                        icon: "image://theme/icon-m-date"
                    },
                    {
                        title: qsTr("Notes"),
                        icon: "image://theme/icon-m-levels"
                    },
                    {
                        title: qsTr("News"),
                        icon: "image://theme/icon-m-note"
                    }
                ]

                delegate: BackgroundItem {
                    id: tabButton
                    width: tabBar.width / 5
                    height: tabBar.height
                    highlighted: page.currentTab === index
                    onClicked: page.showTab(index)

                    Column {
                        anchors.centerIn: parent
                        spacing: Theme.paddingSmall / 2

                        Item {
                            width: Theme.iconSizeMedium
                            height: Theme.iconSizeMedium
                            anchors.horizontalCenter: parent.horizontalCenter

                            Image {
                                anchors.centerIn: parent
                                source: modelData.icon
                                sourceSize.width: Theme.iconSizeMedium
                                sourceSize.height: Theme.iconSizeMedium
                                opacity: tabButton.highlighted ? 1 : 0.7
                            }

                            Rectangle {
                                visible: (index === 1 && wilmaClient.unreadCount > 0)
                                         || (index === 3 && wilmaClient.lessonNotesActionCount > 0)
                                anchors {
                                    right: parent.right
                                    top: parent.top
                                    rightMargin: -Theme.paddingSmall
                                }
                                width: Math.max(Theme.paddingLarge, badgeLabel.implicitWidth + Theme.paddingSmall)
                                height: Theme.paddingLarge
                                radius: height / 2
                                color: Theme.highlightColor

                                Label {
                                    id: badgeLabel
                                    anchors.centerIn: parent
                                    color: Theme.highlightDimmerColor
                                    font.pixelSize: Theme.fontSizeTiny
                                    font.bold: true
                                    text: {
                                        var n = index === 3
                                                ? wilmaClient.lessonNotesActionCount
                                                : wilmaClient.unreadCount
                                        return n > 99 ? "99+" : String(n)
                                    }
                                }
                            }
                        }

                        Label {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: modelData.title
                            font.pixelSize: Theme.fontSizeTiny
                            color: tabButton.highlighted ? Theme.highlightColor : Theme.secondaryColor
                        }
                    }
                }
            }
        }
    }
}
