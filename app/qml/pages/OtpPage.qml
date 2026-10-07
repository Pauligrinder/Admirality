import QtQuick 2.6
import Sailfish.Silica 1.0

Page {
    id: page
    objectName: "OtpPage"
    backNavigation: !wilmaClient.busy
    allowedOrientations: Orientation.All

    function submit() {
        wilmaClient.submitOtp(codeField.text)
    }

    Connections {
        target: wilmaClient
        onLoginSucceeded: pageStack.replaceAbove(null, Qt.resolvedUrl("MainPage.qml"))
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        VerticalScrollDecorator {}

        Column {
            id: column
            width: parent.width
            spacing: Theme.paddingLarge

            PageHeader { title: qsTr("Verification") }

            Label {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeSmall
                text: qsTr("Enter the one-time code from your authenticator app.")
            }

            TextField {
                id: codeField
                width: parent.width
                label: qsTr("One-time code")
                placeholderText: "123456"
                inputMethodHints: Qt.ImhDigitsOnly
                EnterKey.enabled: text.length >= 6 && !wilmaClient.busy
                EnterKey.iconSource: "image://theme/icon-m-enter-accept"
                EnterKey.onClicked: page.submit()
            }

            Label {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                color: Theme.highlightColor
                font.pixelSize: Theme.fontSizeExtraSmall
                visible: wilmaClient.errorMessage.length > 0
                text: wilmaClient.errorMessage
            }

            BusyIndicator {
                anchors.horizontalCenter: parent.horizontalCenter
                running: wilmaClient.busy
                visible: running
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: wilmaClient.busy ? qsTr("Verifying…") : qsTr("Verify")
                enabled: codeField.text.length >= 6 && !wilmaClient.busy
                onClicked: page.submit()
            }
        }
    }
}
