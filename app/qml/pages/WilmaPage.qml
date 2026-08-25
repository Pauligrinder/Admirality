import QtQuick 2.6
import Sailfish.Silica 1.0
import Sailfish.WebView 1.0

WebViewPage {
    id: page
    objectName: "WilmaPage"
    backNavigation: false
    allowedOrientations: Orientation.All

    property bool pageLoaded: false
    property bool loginInjected: false

    function jsString(value) {
        return JSON.stringify(value ? String(value) : "")
    }

    function reloadWilma() {
        page.pageLoaded = false
        page.loginInjected = false
        wilmaView.reload()
    }

    function injectNativeLogin() {
        if (page.loginInjected || !wilmaClient.hasCredentials)
            return
        var script = "return (function(){"
                + "var u=document.querySelector('input[name=Login]');"
                + "var p=document.querySelector('input[name=Password]');"
                + "var f=u&&u.form?u.form:document.querySelector('form');"
                + "if(!u||!p||!f)return 'no-form';"
                + "u.value=" + page.jsString(wilmaClient.username) + ";"
                + "p.value=" + page.jsString(wilmaClient.password) + ";"
                + "if(typeof f.submit==='function')f.submit();"
                + "return 'submitted';"
                + "})();"
        wilmaView.runJavaScript(script, function(result) {
            if (result === "submitted")
                page.loginInjected = true
        })
    }

    WebView {
        id: wilmaView
        anchors.fill: parent
        url: wilmaClient.schoolUrl
        onLoadedChanged: {
            if (!loaded)
                return
            page.pageLoaded = true
            var value = String(url)
            if (value.indexOf("/login") >= 0 || value.indexOf("loginfailed") >= 0)
                page.injectNativeLogin()
        }
        onUrlChanged: {
            var value = String(url)
            if (value.indexOf("/login") >= 0 || value.indexOf("loginfailed") >= 0)
                page.injectNativeLogin()
        }
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
