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

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Change Wilma")
                onClicked: {
                    wilmaClient.logout()
                    pageStack.replaceAbove(null, Qt.resolvedUrl("SchoolPickerPage.qml"))
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
