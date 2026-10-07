import QtQuick 2.6
import Sailfish.Silica 1.0

Item {
    id: page
    objectName: "NewsPage"
    anchors.fill: parent

    Component.onCompleted: wilmaClient.acknowledgeNews()

    SilicaListView {
        id: list
        anchors.fill: parent
        currentIndex: -1
        model: wilmaClient.news

        header: PageHeader {
            title: qsTr("News")
            description: wilmaClient.roleName.length > 0
                         ? wilmaClient.roleName
                         : wilmaClient.displayName
        }

        PullDownMenu {
            busy: wilmaClient.refreshing
            MenuItem {
                text: qsTr("Settings")
                onClicked: pageStack.push(Qt.resolvedUrl("SettingsPage.qml"))
            }
            MenuItem {
                text: qsTr("Open Wilma site")
                onClicked: pageStack.push(Qt.resolvedUrl("WilmaPage.qml"))
            }
            MenuItem {
                visible: wilmaClient.roles.length > 1
                text: qsTr("Change user")
                onClicked: pageStack.push(Qt.resolvedUrl("RolePickerPage.qml"))
            }
            MenuItem {
                text: qsTr("Refresh")
                onClicked: wilmaClient.refreshHome()
            }
        }

        delegate: ListItem {
            id: item
            width: list.width
            contentHeight: col.height + Theme.paddingMedium * 2
            onClicked: pageStack.push(Qt.resolvedUrl("NewsDetailPage.qml"), {
                                          newsId: modelData.id,
                                          title: modelData.title || ""
                                      })

            Column {
                id: col
                anchors {
                    left: parent.left
                    right: parent.right
                    verticalCenter: parent.verticalCenter
                    leftMargin: Theme.horizontalPageMargin
                    rightMargin: Theme.horizontalPageMargin
                }

                Label {
                    width: parent.width
                    wrapMode: Text.Wrap
                    text: modelData.title || qsTr("News")
                    color: item.highlighted ? Theme.highlightColor : Theme.primaryColor
                }

                Label {
                    width: parent.width
                    truncationMode: TruncationMode.Fade
                    visible: (modelData.published || modelData.author || modelData.subtitle || "").length > 0
                    color: item.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
                    font.pixelSize: Theme.fontSizeExtraSmall
                    text: {
                        var parts = []
                        if (modelData.published)
                            parts.push(modelData.published)
                        if (modelData.author)
                            parts.push(modelData.author)
                        else if (modelData.subtitle)
                            parts.push(modelData.subtitle)
                        return parts.join(" · ")
                    }
                }
            }
        }

        ViewPlaceholder {
            enabled: wilmaClient.news.length === 0
            text: wilmaClient.refreshing ? qsTr("Loading…") : qsTr("No news")
        }

        VerticalScrollDecorator {}
    }
}
