import QtQuick 2.6
import Sailfish.Silica 1.0
import Sailfish.WebView 1.0

WebViewPage {
    id: page
    objectName: "WilmaPage"
    backNavigation: false
    allowedOrientations: Orientation.All

    property string startUrl: appWindow.wilmaUrl
    property bool pageLoaded: false

    function reloadWilma() {
        page.pageLoaded = false
        wilmaView.reload()
    }

    WebView {
        id: wilmaView
        anchors.fill: parent
        url: page.startUrl
        onLoadedChanged: {
            if (loaded)
                page.pageLoaded = true
        }
        onUrlChanged: appWindow.rememberWilmaUrlFromNavigation(String(url))
    }

    Rectangle {
        id: loadingOverlay
        anchors.fill: parent
        color: Theme.highlightDimmerColor
        visible: !page.pageLoaded
        z: 2

        Column {
            anchors.centerIn: parent
            width: parent.width - 2 * Theme.horizontalPageMargin
            spacing: Theme.paddingLarge

            BusyIndicator {
                anchors.horizontalCenter: parent.horizontalCenter
                running: loadingOverlay.visible
                size: BusyIndicatorSize.Large
            }

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                color: Theme.highlightColor
                font.pixelSize: Theme.fontSizeSmall
                text: wilmaView.loading && wilmaView.loadProgress > 0
                      ? qsTr("Loading Wilma… %1%").arg(wilmaView.loadProgress)
                      : qsTr("Loading Wilma…")
            }
        }
    }
}
