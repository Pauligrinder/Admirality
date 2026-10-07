import QtQuick 2.6
import Sailfish.Silica 1.0

Page {
    id: page
    objectName: "SchoolPickerPage"
    allowedOrientations: Orientation.All

    property string query: ""
    property string customText: ""
    property var filteredSchools: {
        var all = wilmaClient.schools || []
        var q = page.query
        if (!q.length)
            return all
        var out = []
        for (var i = 0; i < all.length; ++i) {
            if (String(all[i].searchText).indexOf(q) >= 0)
                out.push(all[i])
        }
        return out
    }

    function chooseSchool(url, name) {
        wilmaClient.selectSchool(url, name)
        pageStack.replaceAbove(null, Qt.resolvedUrl("LoginPage.qml"))
    }

    function chooseCustom() {
        var url = wilmaClient.normalizeSchoolUrl(page.customText)
        if (!url.length)
            return
        wilmaClient.selectSchoolUrl(url)
        pageStack.replaceAbove(null, Qt.resolvedUrl("LoginPage.qml"))
    }

    // Keep SearchField outside the list so model resets don't recreate it
    // and dismiss the keyboard after a couple of characters.
    Column {
        id: headerColumn
        width: parent.width

        PageHeader { title: qsTr("Choose Wilma") }

        Label {
            x: Theme.horizontalPageMargin
            width: parent.width - Theme.horizontalPageMargin * 2
            wrapMode: Text.Wrap
            color: Theme.secondaryColor
            font.pixelSize: Theme.fontSizeSmall
            text: qsTr("Pick your city or school. The list comes from the same public Wilma directory used by open-source clients such as wilmai.")
        }

        SearchField {
            id: searchField
            width: parent.width
            placeholderText: qsTr("City or school")
            onTextChanged: page.query = text.trim().toLowerCase()
        }
    }

    SilicaListView {
        id: list
        anchors {
            top: headerColumn.bottom
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        clip: true
        currentIndex: -1
        model: page.filteredSchools

        delegate: ListItem {
            id: schoolItem
            width: list.width
            contentHeight: Theme.itemSizeMedium
            onClicked: page.chooseSchool(modelData.url, modelData.name)

            Column {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: Theme.horizontalPageMargin
                anchors.rightMargin: Theme.horizontalPageMargin

                Label {
                    width: parent.width
                    truncationMode: TruncationMode.Fade
                    text: modelData.name
                    color: schoolItem.highlighted ? Theme.highlightColor : Theme.primaryColor
                }

                Label {
                    width: parent.width
                    truncationMode: TruncationMode.Fade
                    visible: modelData.cities && modelData.cities.length > 0
                    text: modelData.cities || ""
                    color: schoolItem.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
                    font.pixelSize: Theme.fontSizeExtraSmall
                }
            }
        }

        footer: Column {
            width: list.width
            spacing: Theme.paddingMedium

            SectionHeader { text: qsTr("Custom address") }

            TextField {
                width: parent.width
                label: qsTr("Wilma address")
                placeholderText: "espoo.inschool.fi"
                text: page.customText
                onTextChanged: page.customText = text
                inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase | Qt.ImhUrlCharactersOnly
                EnterKey.enabled: text.trim().length > 0
                EnterKey.iconSource: "image://theme/icon-m-enter-accept"
                EnterKey.onClicked: page.chooseCustom()
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Use this address")
                enabled: page.customText.trim().length > 0
                onClicked: page.chooseCustom()
            }

            Item { width: 1; height: Theme.paddingLarge }
        }

        ViewPlaceholder {
            enabled: page.filteredSchools.length === 0
            text: qsTr("No matching Wilma")
        }

        VerticalScrollDecorator {}
    }
}
