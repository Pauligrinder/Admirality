import QtQuick 2.6
import Sailfish.Silica 1.0

Page {
    id: page
    objectName: "RolePickerPage"
    allowedOrientations: Orientation.All

    SilicaListView {
        id: list
        anchors.fill: parent
        currentIndex: -1
        model: wilmaClient.roles

        header: PageHeader {
            title: qsTr("Change user")
            description: wilmaClient.roleName.length > 0
                         ? qsTr("Current: %1").arg(wilmaClient.roleName)
                         : ""
        }

        ViewPlaceholder {
            enabled: wilmaClient.roles.length === 0
            text: qsTr("No users available")
        }

        delegate: BackgroundItem {
            id: item
            width: list.width
            height: Theme.itemSizeMedium
            highlighted: down || String(modelData.id) === String(wilmaClient.roleId)
            onClicked: {
                wilmaClient.selectRole(String(modelData.id))
                pageStack.pop()
            }

            Label {
                anchors {
                    left: parent.left
                    right: check.left
                    verticalCenter: parent.verticalCenter
                    leftMargin: Theme.horizontalPageMargin
                    rightMargin: Theme.paddingMedium
                }
                truncationMode: TruncationMode.Fade
                color: item.highlighted ? Theme.highlightColor : Theme.primaryColor
                text: modelData.name || modelData.id
            }

            Image {
                id: check
                anchors {
                    right: parent.right
                    verticalCenter: parent.verticalCenter
                    rightMargin: Theme.horizontalPageMargin
                }
                width: Theme.iconSizeMedium
                height: Theme.iconSizeMedium
                visible: String(modelData.id) === String(wilmaClient.roleId)
                source: "image://theme/icon-m-acknowledge"
            }
        }

        VerticalScrollDecorator {}
    }
}
