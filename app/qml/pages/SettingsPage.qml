import QtQuick 2.6
import Sailfish.Silica 1.0

Page {
    id: page
    objectName: "SettingsPage"
    allowedOrientations: Orientation.All

    function applyAndOpen() {
        var url = appWindow.normalizeWilmaUrl(urlField.text)
        if (!url.length)
            return
        appWindow.setWilmaUrl(url)
        pageStack.replaceAbove(null, Qt.resolvedUrl("WilmaPage.qml"))
    }

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
                text: qsTr("Admirality wraps Wilma / Inschool.fi in a Sailfish WebView. "
                           + "Enter your school's Wilma address. A short name such as "
                           + "“espoo” becomes https://espoo.inschool.fi.")
            }

            TextField {
                id: urlField
                width: parent.width
                label: qsTr("Wilma address")
                placeholderText: "espoo.inschool.fi"
                text: appWindow.wilmaUrl
                inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase | Qt.ImhUrlCharactersOnly
                EnterKey.enabled: text.trim().length > 0
                EnterKey.iconSource: "image://theme/icon-m-enter-accept"
                EnterKey.onClicked: page.applyAndOpen()
            }

            Label {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                color: Theme.secondaryHighlightColor
                font.pixelSize: Theme.fontSizeExtraSmall
                visible: urlField.text.trim().length > 0
                text: qsTr("Will open %1").arg(appWindow.normalizeWilmaUrl(urlField.text))
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Open Wilma")
                enabled: urlField.text.trim().length > 0
                onClicked: page.applyAndOpen()
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Find school on inschool.fi")
                onClicked: {
                    appWindow.setWilmaUrl("https://inschool.fi")
                    pageStack.replaceAbove(null, Qt.resolvedUrl("WilmaPage.qml"))
                }
            }

            Label {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeExtraSmall
                text: qsTr("When you open a school from inschool.fi, Admirality stores that "
                           + "Wilma address for the next launch.")
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
