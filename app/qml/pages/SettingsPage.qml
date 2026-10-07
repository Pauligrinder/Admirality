import QtQuick 2.6
import Sailfish.Silica 1.0

Page {
    id: page
    objectName: "SettingsPage"
    allowedOrientations: Orientation.All

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        VerticalScrollDecorator {}

        Column {
            id: column
            width: parent.width
            spacing: Theme.paddingLarge

            PageHeader { title: qsTr("Admirality") }

            Label {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeSmall
                text: wilmaClient.hasSchool
                      ? qsTr("Signed in to %1 as %2.")
                        .arg(wilmaClient.schoolName.length > 0
                             ? wilmaClient.schoolName
                             : wilmaClient.schoolHost)
                        .arg(wilmaClient.displayName.length > 0
                             ? wilmaClient.displayName
                             : wilmaClient.username)
                      : qsTr("No Wilma selected.")
            }

            Label {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                color: Theme.secondaryHighlightColor
                font.pixelSize: Theme.fontSizeExtraSmall
                visible: wilmaClient.loggedIn
                text: wilmaClient.unreadCount === 1
                      ? qsTr("1 unread message")
                      : qsTr("%1 unread messages").arg(wilmaClient.unreadCount)
            }

            SectionHeader {
                visible: wilmaClient.roles.length > 1
                text: qsTr("Account")
            }

            Repeater {
                model: wilmaClient.roles
                delegate: BackgroundItem {
                    width: column.width
                    height: Theme.itemSizeMedium
                    visible: wilmaClient.roles.length > 1
                    highlighted: String(modelData.id) === String(wilmaClient.roleId)
                    onClicked: wilmaClient.selectRole(String(modelData.id))

                    Label {
                        anchors {
                            left: parent.left
                            right: parent.right
                            verticalCenter: parent.verticalCenter
                            leftMargin: Theme.horizontalPageMargin
                            rightMargin: Theme.horizontalPageMargin
                        }
                        truncationMode: TruncationMode.Fade
                        color: parent.highlighted ? Theme.highlightColor : Theme.primaryColor
                        text: modelData.name || modelData.id
                    }
                }
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Open Wilma site")
                enabled: wilmaClient.hasSchool
                onClicked: pageStack.push(Qt.resolvedUrl("WilmaPage.qml"))
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Change Wilma")
                onClicked: pageStack.replaceAbove(null, Qt.resolvedUrl("SchoolPickerPage.qml"))
            }

            ComboBox {
                id: pollBox
                width: parent.width
                label: qsTr("Check Wilma")
                description: qsTr("Before and after the school day means 15 minutes before the first lesson and 15 minutes after the last one.")

                property bool applying: false
                property var modes: ["15min", "hour", "3hours", "schoolday"]

                function sync() {
                    var mode = wilmaClient.pollMode || "15min"
                    var index = 0
                    for (var i = 0; i < modes.length; ++i) {
                        if (modes[i] === mode)
                            index = i
                    }
                    if (currentIndex === index)
                        return
                    applying = true
                    currentIndex = index
                    applying = false
                }

                menu: ContextMenu {
                    MenuItem { text: qsTr("Every 15 minutes") }
                    MenuItem { text: qsTr("Once an hour") }
                    MenuItem { text: qsTr("Every 3 hours") }
                    MenuItem { text: qsTr("Before and after the school day") }
                }
                Component.onCompleted: sync()
                Connections {
                    target: wilmaClient
                    onPollModeChanged: pollBox.sync()
                }
                onCurrentIndexChanged: {
                    if (applying)
                        return
                    var mode = modes[currentIndex] || "15min"
                    if (mode !== wilmaClient.pollMode)
                        wilmaClient.setPollMode(mode)
                }
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Sign out")
                enabled: wilmaClient.hasCredentials || wilmaClient.loggedIn
                onClicked: {
                    wilmaClient.logout()
                    pageStack.replaceAbove(null, Qt.resolvedUrl("LoginPage.qml"))
                }
            }

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeExtraSmall
                text: qsTr("App %1").arg(appVersion)
            }
        }
    }
}
