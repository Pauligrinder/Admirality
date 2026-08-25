import QtQuick 2.6
import Sailfish.Silica 1.0
import Qt.labs.settings 1.0
import "cover" as CoverDir
import "pages"

ApplicationWindow
{
    id: appWindow

    Settings {
        id: appSettings
        property string wilmaUrl: ""
    }

    readonly property string wilmaUrl: appSettings.wilmaUrl
    readonly property bool hasWilmaUrl: appWindow.wilmaUrl.length > 0
    readonly property string schoolHost: appWindow.hostFromUrl(appWindow.wilmaUrl)
    readonly property string schoolName: appWindow.displayNameFromHost(appWindow.schoolHost)

    function trimSlash(value) {
        var s = String(value || "").trim()
        while (s.length > 1 && s.charAt(s.length - 1) === "/")
            s = s.substring(0, s.length - 1)
        return s
    }

    function normalizeWilmaUrl(raw) {
        var s = String(raw || "").trim()
        if (!s.length)
            return ""
        if (s.indexOf("://") < 0) {
            if (s.indexOf(".") < 0)
                s = s + ".inschool.fi"
            s = "https://" + s
        }
        return appWindow.trimSlash(s)
    }

    function hostFromUrl(value) {
        var s = String(value || "")
        var withoutScheme = s.replace(/^[a-zA-Z][a-zA-Z0-9+.-]*:\/\//, "")
        var host = withoutScheme.split("/")[0].split(":")[0]
        return host.toLowerCase()
    }

    function displayNameFromHost(host) {
        if (!host || host.length === 0)
            return ""
        if (host === "inschool.fi" || host === "www.inschool.fi")
            return "Inschool.fi"
        if (host.indexOf(".inschool.fi") > 0)
            return host.substring(0, host.length - ".inschool.fi".length)
        return host
    }

    function isSchoolWilmaHost(host) {
        if (!host || host.length === 0)
            return false
        if (host === "inschool.fi" || host === "www.inschool.fi")
            return false
        return host.indexOf(".inschool.fi") > 0
    }

    function originFromUrl(value) {
        var s = String(value || "")
        var match = s.match(/^[a-zA-Z][a-zA-Z0-9+.-]*:\/\/[^\/]+/)
        if (!match)
            return appWindow.trimSlash(s)
        return match[0]
    }

    function setWilmaUrl(raw) {
        appSettings.wilmaUrl = appWindow.normalizeWilmaUrl(raw)
    }

    function rememberWilmaUrlFromNavigation(value) {
        var origin = appWindow.originFromUrl(value)
        var host = appWindow.hostFromUrl(origin)
        if (!appWindow.isSchoolWilmaHost(host))
            return
        if (appSettings.wilmaUrl === origin)
            return
        appSettings.wilmaUrl = origin
    }

    function openSettings() {
        if (pageStack.currentPage && pageStack.currentPage.objectName === "SettingsPage")
            return
        pageStack.push(Qt.resolvedUrl("pages/SettingsPage.qml"))
    }

    initialPage: Component {
        FirstPage { }
    }
    cover: CoverDir.CoverPage {
        schoolName: appWindow.schoolName
        schoolHost: appWindow.schoolHost
        configured: appWindow.hasWilmaUrl
        onRequestSettings: {
            appWindow.openSettings()
            appWindow.activate()
        }
        onRequestReload: {
            var page = pageStack.currentPage
            if (page && typeof page.reloadWilma === "function")
                page.reloadWilma()
            appWindow.activate()
        }
    }
    allowedOrientations: Orientation.All
}
