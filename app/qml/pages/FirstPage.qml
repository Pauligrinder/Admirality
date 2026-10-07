import QtQuick 2.6
import Sailfish.Silica 1.0

Page {
    id: page
    objectName: "SplashPage"
    backNavigation: false

    property bool finished: false
    property int bootWaits: 0

    function finish(loggedIn) {
        // Always allow a successful restore to take over even if the safety
        // timer already pushed LoginPage (common after a slow re-auth on boot).
        if (page.finished && !loggedIn)
            return
        page.finished = true

        if (loggedIn) {
            pageStack.replaceAbove(null, Qt.resolvedUrl("MainPage.qml"))
            // Events widget may have requested a page before splash finished.
            if (appWindow.pendingOpenView && appWindow.pendingOpenView.length)
                appWindow.openWilmaView(appWindow.pendingOpenView)
            return
        }
        if (wilmaClient.hasSchool)
            pageStack.replaceAbove(null, Qt.resolvedUrl("LoginPage.qml"))
        else
            pageStack.replaceAbove(null, Qt.resolvedUrl("SchoolPickerPage.qml"))
    }

    // Defer until pageStack has installed this as initialPage.
    Timer {
        id: bootTimer
        interval: 1
        running: true
        repeat: false
        onTriggered: {
            if (!wilmaClient.serviceReady) {
                page.bootWaits += 1
                if (page.bootWaits < 40) {
                    bootTimer.interval = 200
                    bootTimer.restart()
                    return
                }
            }
            if (!wilmaClient.hasSchool) {
                page.finish(false)
                return
            }
            // School is already chosen — go to login. Only try a silent
            // re-auth when we actually have stored credentials.
            if (!wilmaClient.hasCredentials) {
                page.finish(false)
                return
            }
            wilmaClient.restoreSession()
        }
    }

    Timer {
        interval: 20000
        running: !page.finished
        repeat: false
        onTriggered: {
            // Do not abort an in-flight restore/login — wait for its signal.
            if (wilmaClient.restoringSession || wilmaClient.busy)
                return
            page.finish(wilmaClient.loggedIn)
        }
    }

    Connections {
        target: wilmaClient
        onRestoreFinished: page.finish(loggedIn)
        onLoginSucceeded: page.finish(true)
        onLoggedInChanged: {
            if (wilmaClient.loggedIn)
                page.finish(true)
        }
        onOtpRequired: {
            page.finished = true
            pageStack.replaceAbove(null, Qt.resolvedUrl("OtpPage.qml"))
        }
    }

    Column {
        anchors.centerIn: parent
        width: parent.width - 2 * Theme.horizontalPageMargin
        spacing: Theme.paddingLarge

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("Admirality")
            color: Theme.highlightColor
            font.pixelSize: Theme.fontSizeExtraLarge
        }

        BusyIndicator {
            anchors.horizontalCenter: parent.horizontalCenter
            running: !page.finished
            size: BusyIndicatorSize.Large
        }

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            color: Theme.secondaryColor
            font.pixelSize: Theme.fontSizeSmall
            text: wilmaClient.statusText.length > 0
                  ? wilmaClient.statusText
                  : qsTr("Starting…")
        }
    }
}
