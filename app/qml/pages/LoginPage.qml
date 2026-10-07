import QtQuick 2.6
import Sailfish.Silica 1.0

Page {
    id: page
    objectName: "LoginPage"
    allowedOrientations: Orientation.All

    property bool autoSubmitted: false

    function submit() {
        var user = usernameField.text.length > 0 ? usernameField.text : wilmaClient.username
        var pass = passwordField.text.length > 0 ? passwordField.text : wilmaClient.password
        wilmaClient.login(user, pass)
    }

    function tryAutoLogin() {
        if (page.autoSubmitted || wilmaClient.busy || wilmaClient.loggedIn
                || wilmaClient.restoringSession)
            return
        if (!wilmaClient.hasCredentials)
            return
        page.autoSubmitted = true
        // Use stored credentials directly — PasswordField may not echo saved text.
        wilmaClient.login(wilmaClient.username, wilmaClient.password)
    }

    Connections {
        target: wilmaClient
        onLoginSucceeded: pageStack.replaceAbove(null, Qt.resolvedUrl("MainPage.qml"))
        onOtpRequired: pageStack.replaceAbove(null, Qt.resolvedUrl("OtpPage.qml"))
    }

    Component.onCompleted: autoLoginTimer.start()

    Timer {
        id: autoLoginTimer
        interval: 50
        repeat: false
        onTriggered: page.tryAutoLogin()
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        VerticalScrollDecorator {}

        PullDownMenu {
            MenuItem {
                text: qsTr("Change Wilma")
                onClicked: pageStack.replaceAbove(null, Qt.resolvedUrl("SchoolPickerPage.qml"))
            }
        }

        Column {
            id: column
            width: parent.width
            spacing: Theme.paddingLarge

            PageHeader { title: qsTr("Sign in") }

            Label {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeSmall
                text: qsTr("Sign in to %1 with your Wilma username and password.")
                      .arg(wilmaClient.schoolName.length > 0
                           ? wilmaClient.schoolName
                           : wilmaClient.schoolHost)
            }

            TextField {
                id: usernameField
                width: parent.width
                label: qsTr("Username")
                placeholderText: qsTr("Username")
                text: wilmaClient.username
                inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                EnterKey.iconSource: "image://theme/icon-m-enter-next"
                EnterKey.onClicked: passwordField.forceActiveFocus()
            }

            PasswordField {
                id: passwordField
                width: parent.width
                label: qsTr("Password")
                placeholderText: qsTr("Password")
                text: wilmaClient.password
                EnterKey.enabled: usernameField.text.length > 0 && text.length > 0 && !wilmaClient.busy
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

            Label {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                color: Theme.secondaryHighlightColor
                font.pixelSize: Theme.fontSizeExtraSmall
                text: wilmaClient.statusText
            }

            BusyIndicator {
                anchors.horizontalCenter: parent.horizontalCenter
                running: wilmaClient.busy
                visible: running
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: wilmaClient.busy ? qsTr("Signing in…") : qsTr("Sign in")
                enabled: usernameField.text.length > 0
                         && passwordField.text.length > 0
                         && !wilmaClient.busy
                onClicked: page.submit()
            }
        }
    }
}
