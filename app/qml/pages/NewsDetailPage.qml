import QtQuick 2.6
import Sailfish.Silica 1.0

Page {
    id: page
    objectName: "NewsDetailPage"
    allowedOrientations: Orientation.All

    property int newsId: 0
    property string title: ""

    readonly property var item: wilmaClient.currentNews
    readonly property bool ready: item && item.id === page.newsId && !wilmaClient.detailBusy

    Component.onCompleted: wilmaClient.openNews(page.newsId)

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
                title: page.ready && page.item.title
                       ? page.item.title
                       : (page.title || qsTr("News"))
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeSmall
                visible: page.ready && (page.item.subtitle || page.item.author || page.item.published)
                text: {
                    if (!page.ready)
                        return ""
                    var parts = []
                    if (page.item.published)
                        parts.push(page.item.published)
                    if (page.item.author)
                        parts.push(page.item.author)
                    if (page.item.subtitle)
                        parts.push(page.item.subtitle)
                    return parts.join(" · ")
                }
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
