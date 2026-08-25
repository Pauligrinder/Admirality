import QtQuick 2.6
import Sailfish.Silica 1.0

Page {
    id: page
    objectName: "SplashPage"
    backNavigation: false

    Component.onCompleted: {
        if (appWindow.hasWilmaUrl)
            pageStack.replaceAbove(null, Qt.resolvedUrl("WilmaPage.qml"))
        else
            pageStack.replaceAbove(null, Qt.resolvedUrl("SettingsPage.qml"))
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
            running: true
            size: BusyIndicatorSize.Large
        }
    }
}
