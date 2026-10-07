import QtQuick 2.6
import Sailfish.Silica 1.0
import Sailfish.WebView 1.0

WebViewPage {
    id: page
    objectName: "WilmaPage"
    backNavigation: true
    allowedOrientations: Orientation.All

    property bool pageLoaded: false
    property bool loginInjected: false
    property int loginAttempts: 0

    function jsString(value) {
        return JSON.stringify(value ? String(value) : "")
    }

    function isLoginUrl(value) {
        var text = String(value)
        return text.indexOf("/login") >= 0 || text.indexOf("loginfailed") >= 0
    }

    function reloadWilma() {
        page.pageLoaded = false
        page.loginInjected = false
        page.loginAttempts = 0
        wilmaView.url = page.wilmaEntryUrl()
    }

    function wilmaEntryUrl() {
        // Land on /login when we have credentials so the WebView can establish
        // its own HttpOnly session via the real form (document.cookie cannot).
        if (wilmaClient.hasCredentials && wilmaClient.schoolUrl)
            return wilmaClient.schoolUrl + "/login"
        return wilmaClient.schoolUrl
    }

    function injectNativeLogin() {
        if (page.loginInjected || !wilmaClient.hasCredentials)
            return
        if (page.loginAttempts >= 8)
            return
        page.loginAttempts += 1

        var script = "return (function(){"
                + "var u=document.querySelector('input[name=Login]');"
                + "var p=document.querySelector('input[name=Password]');"
                + "var f=u&&u.form?u.form:document.querySelector('form#loginForm,form');"
                + "if(!u||!p||!f)return 'no-form';"
                + "u.value=" + page.jsString(wilmaClient.username) + ";"
                + "p.value=" + page.jsString(wilmaClient.password) + ";"
                + "if(typeof f.requestSubmit==='function'){f.requestSubmit();return 'submitted';}"
                + "if(typeof f.submit==='function'){f.submit();return 'submitted';}"
                + "return 'no-submit';"
                + "})();"

        wilmaView.runJavaScript(script, function(result) {
            if (result === "submitted") {
                page.loginInjected = true
                return
            }
            // Form not ready yet — try again shortly.
            if (!page.loginInjected)
                retryLoginTimer.restart()
        })
    }

    function onWilmaReady() {
        page.pageLoaded = true
        if (page.isLoginUrl(wilmaView.url)) {
            page.injectNativeLogin()
            return
        }
        // Past the login gate — WebView has its own session cookie now.
        page.loginInjected = true
        page.loginAttempts = 0
    }

    Timer {
        id: retryLoginTimer
        interval: 450
        repeat: false
        onTriggered: {
            if (!page.loginInjected && page.isLoginUrl(wilmaView.url))
                page.injectNativeLogin()
        }
    }

    WebView {
        id: wilmaView
        anchors {
            top: header.bottom
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        url: page.wilmaEntryUrl()
        onLoadedChanged: {
            if (loaded)
                page.onWilmaReady()
        }
        onUrlChanged: {
            if (page.isLoginUrl(url)) {
                // New login page (e.g. after expiry) — allow another inject.
                page.loginInjected = false
                page.loginAttempts = 0
            }
            if (wilmaView.loaded)
                page.onWilmaReady()
        }
    }

    PageHeader {
        id: header
        title: qsTr("Wilma site")
    }

    Rectangle {
        id: loadingOverlay
        anchors {
            top: header.bottom
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
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
                      : (page.isLoginUrl(wilmaView.url)
                         ? qsTr("Signing in to Wilma…")
                         : qsTr("Loading Wilma…"))
            }
        }
    }
}
