import QtQuick 2.6
import Sailfish.Silica 1.0

Page {
    id: page
    objectName: "MessagePage"
    allowedOrientations: Orientation.All

    property int messageId: 0
    property string subject: ""
    property string sender: ""

    readonly property var item: wilmaClient.currentMessage
    readonly property bool ready: item && item.id === page.messageId && !wilmaClient.detailBusy

    Component.onCompleted: wilmaClient.openMessage(page.messageId)

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        VerticalScrollDecorator {}

        PullDownMenu {
            MenuItem {
                text: qsTr("Open Wilma site")
                onClicked: pageStack.push(Qt.resolvedUrl("WilmaPage.qml"))
            }
        }

        Column {
            id: column
            width: parent.width
            spacing: Theme.paddingMedium

            PageHeader {
                title: page.ready && page.item.subject
                       ? page.item.subject
                       : (page.subject || qsTr("Message"))
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeSmall
                visible: (page.ready ? page.item.sender : page.sender).length > 0
                text: page.ready && page.item.sender ? page.item.sender : page.sender
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                color: Theme.secondaryHighlightColor
                font.pixelSize: Theme.fontSizeExtraSmall
                visible: page.ready && page.item.time
                text: page.ready ? (page.item.time || "") : ""
            }

            BusyIndicator {
                anchors.horizontalCenter: parent.horizontalCenter
                running: wilmaClient.detailBusy
                visible: running
                size: BusyIndicatorSize.Medium
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                color: Theme.primaryColor
                font.pixelSize: Theme.fontSizeSmall
                visible: page.ready
                text: page.ready ? (page.item.content || "") : ""
            }
        }
    }
}
