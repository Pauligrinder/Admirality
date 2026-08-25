import QtQuick 2.6
import Sailfish.Silica 1.0

Page {
    id: page
    objectName: "SplashPage"
    backNavigation: false

    function finish(loggedIn) {
        if (loggedIn) {
            pageStack.replaceAbove(null, Qt.resolvedUrl("WilmaPage.qml"))
            return
        }
        if (wilmaClient.hasSchool)
            pageStack.replaceAbove(null, Qt.resolvedUrl("LoginPage.qml"))
        else
            pageStack.replaceAbove(null, Qt.resolvedUrl("SchoolPickerPage.qml"))
    }

    Timer {
        interval: 20000
        running: true
        repeat: false
        onTriggered: {
            if (pageStack.currentPage === page)
                page.finish(wilmaClient.loggedIn)
        }
    }

    Connections {
        target: wilmaClient
        onRestoreFinished: page.finish(loggedIn)
        onLoginSucceeded: page.finish(true)
        onOtpRequired: pageStack.replaceAbove(null, Qt.resolvedUrl("OtpPage.qml"))
    }

    Component.onCompleted: wilmaClient.restoreSession()

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
            running: true
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
