import QtQuick 2.6
import Sailfish.Silica 1.0
import Nemo.Notifications 1.0
import "cover" as CoverDir
import "pages"

ApplicationWindow
{
    id: appWindow
    property int notificationCount: 0
    property string coverNotificationTitle: ""
    property string coverNotificationBody: ""

    function goHome() {
        if (pageStack.currentPage && pageStack.currentPage.objectName === "WilmaPage")
            return
        pageStack.replaceAbove(null, Qt.resolvedUrl("pages/WilmaPage.qml"))
    }

    function goLogin() {
        pageStack.replaceAbove(null, Qt.resolvedUrl("pages/LoginPage.qml"))
    }

    function openSettings() {
        if (pageStack.currentPage && pageStack.currentPage.objectName === "SettingsPage")
            return
        pageStack.push(Qt.resolvedUrl("pages/SettingsPage.qml"))
        appWindow.activate()
    }

    function showWilmaNotification(title, message) {
        var n = notificationComponent.createObject(appWindow)
        if (!n)
            return
        var summary = title && title.length > 0 ? title : qsTr("Wilma")
        var body = message || ""
        n.appName = "Admirality"
        n.appIcon = "harbour-admirality"
        n.summary = summary
        n.body = body
        n.previewSummary = summary
        n.previewBody = body
        n.clicked.connect(function() { appWindow.activate() })
        try {
            n.publish()
        } catch (e) {
        }
        appWindow.notificationCount += 1
        appWindow.coverNotificationTitle = summary
        appWindow.coverNotificationBody = body
    }

    Component {
        id: notificationComponent
        Notification { }
    }

    Connections {
        target: wilmaClient
        onLoginSucceeded: appWindow.goHome()
        onOtpRequired: {
            if (pageStack.currentPage
                    && (pageStack.currentPage.objectName === "OtpPage"
                        || pageStack.currentPage.objectName === "SplashPage"))
                return
            pageStack.push(Qt.resolvedUrl("pages/OtpPage.qml"))
        }
        onLoggedInChanged: {
            if (!wilmaClient.loggedIn
                    && pageStack.currentPage
                    && pageStack.currentPage.objectName === "WilmaPage")
                appWindow.goLogin()
        }
        onNotificationReceived: appWindow.showWilmaNotification(title, message)
    }

    onApplicationActiveChanged: {
        if (applicationActive) {
            appWindow.notificationCount = 0
            appWindow.coverNotificationTitle = ""
            appWindow.coverNotificationBody = ""
            if (wilmaClient.loggedIn)
                wilmaClient.pollMessages()
        }
    }

    initialPage: Component {
        FirstPage { }
    }
    cover: CoverDir.CoverPage {
        schoolName: wilmaClient.schoolName
        schoolHost: wilmaClient.schoolHost
        configured: wilmaClient.hasSchool
        loggedIn: wilmaClient.loggedIn
        unreadCount: wilmaClient.unreadCount
        notificationTitle: appWindow.coverNotificationTitle
        notificationBody: appWindow.coverNotificationBody
        showingNotification: appWindow.notificationCount > 0
                             && (appWindow.coverNotificationTitle.length > 0
                                 || appWindow.coverNotificationBody.length > 0)
        onRequestSettings: appWindow.openSettings()
        onRequestReload: {
            if (wilmaClient.loggedIn)
                wilmaClient.pollMessages()
            var page = pageStack.currentPage
            if (page && typeof page.reloadWilma === "function")
                page.reloadWilma()
            appWindow.activate()
        }
    }
    allowedOrientations: Orientation.All
}
