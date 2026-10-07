import QtQuick 2.6
import Sailfish.Silica 1.0

Item {
    id: page
    objectName: "MessagesPage"
    anchors.fill: parent

    property var folders: ["inbox", "outbox", "archive", "drafts"]

    SilicaListView {
        id: list
        anchors.fill: parent
        currentIndex: -1
        model: wilmaClient.messages
        header: Column {
            width: list.width

            PageHeader {
                title: qsTr("Messages")
                description: wilmaClient.roleName.length > 0
                             ? wilmaClient.roleName
                             : (wilmaClient.unreadCount > 0
                                ? qsTr("%1 unread").arg(wilmaClient.unreadCount)
                                : "")
            }

            ComboBox {
                id: folderBox
                width: parent.width
                label: qsTr("Folder")
                currentIndex: {
                    var f = wilmaClient.messageFolder
                    for (var i = 0; i < page.folders.length; ++i) {
                        if (page.folders[i] === f)
                            return i
                    }
                    return 0
                }
                menu: ContextMenu {
                    MenuItem { text: qsTr("Inbox") }
                    MenuItem { text: qsTr("Sent") }
                    MenuItem { text: qsTr("Archive") }
                    MenuItem { text: qsTr("Drafts") }
                }
                onCurrentIndexChanged: {
                    var name = page.folders[currentIndex] || "inbox"
                    if (name !== wilmaClient.messageFolder)
                        wilmaClient.loadMessages(name)
                }
            }
        }

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
                text: qsTr("Refresh")
                onClicked: wilmaClient.refreshHome()
            }
        }

        delegate: ListItem {
            id: item
            width: list.width
            contentHeight: col.height + Theme.paddingMedium * 2
            onClicked: pageStack.push(Qt.resolvedUrl("MessagePage.qml"), {
                                          messageId: modelData.id,
                                          subject: modelData.subject || "",
                                          sender: modelData.sender || ""
                                      })

            Rectangle {
                visible: modelData.unread
                anchors {
                    left: parent.left
                    top: parent.top
                    bottom: parent.bottom
                }
                width: Theme.paddingSmall
                color: Theme.highlightColor
            }

            Column {
                id: col
                anchors {
                    left: parent.left
                    right: parent.right
                    verticalCenter: parent.verticalCenter
                    leftMargin: Theme.horizontalPageMargin
                    rightMargin: Theme.horizontalPageMargin
                }

                Label {
                    width: parent.width
                    truncationMode: TruncationMode.Fade
                    text: modelData.subject || qsTr("Message")
                    font.bold: modelData.unread
                    color: modelData.unread ? Theme.highlightColor
                           : (item.highlighted ? Theme.highlightColor : Theme.primaryColor)
                }

                Label {
                    width: parent.width
                    truncationMode: TruncationMode.Fade
                    color: item.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
                    font.pixelSize: Theme.fontSizeExtraSmall
                    text: {
                        var parts = []
                        if (modelData.sender)
                            parts.push(modelData.sender)
                        if (modelData.time)
                            parts.push(modelData.time)
                        return parts.join(" · ")
                    }
                }
            }
        }

        ViewPlaceholder {
            enabled: wilmaClient.messages.length === 0
            text: wilmaClient.refreshing ? qsTr("Loading…") : qsTr("No messages")
            hintText: wilmaClient.refreshing
                      ? ""
                      : qsTr("Pull down to refresh, or open the Wilma site from the menu.")
        }

        VerticalScrollDecorator {}
    }
}
