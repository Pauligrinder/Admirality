import QtQuick 2.6
import Sailfish.Silica 1.0
import "cover" as CoverDir
import "pages"

ApplicationWindow
{
    id: appWindow
    property int notificationCount: 0
    property string coverNotificationTitle: ""
    property string coverNotificationBody: ""

    function isAuthedPage(name) {
        return name === "MainPage"
                || name === "HomePage"
                || name === "WilmaPage"
                || name === "MessagesPage"
                || name === "MessagePage"
                || name === "NewsPage"
                || name === "NewsDetailPage"
                || name === "SchedulePage"
                || name === "NotesPage"
                || name === "SettingsPage"
    }

    function goHome() {
        var name = pageStack.currentPage ? pageStack.currentPage.objectName : ""
        if (name === "MainPage" || name === "HomePage" || appWindow.isAuthedPage(name))
            return
        pageStack.replaceAbove(null, Qt.resolvedUrl("pages/MainPage.qml"))
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
        var summary = title && title.length > 0 ? title : qsTr("Wilma")
        var body = message || ""
        appWindow.notificationCount += 1
        appWindow.coverNotificationTitle = summary
        appWindow.coverNotificationBody = body
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
                    && appWindow.isAuthedPage(pageStack.currentPage.objectName))
                appWindow.goLogin()
        }
        onNotificationReceived: appWindow.showWilmaNotification(title, message)
    }

    onApplicationActiveChanged: {
        if (applicationActive) {
            appWindow.notificationCount = 0
            appWindow.coverNotificationTitle = ""
            appWindow.coverNotificationBody = ""
            if (wilmaClient.loggedIn && wilmaClient.roleId.length > 0)
                wilmaClient.refreshHome()
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
            if (wilmaClient.loggedIn && wilmaClient.roleId.length > 0)
                wilmaClient.refreshHome()
            var page = pageStack.currentPage
            if (page && page.objectName === "WilmaPage" && typeof page.reloadWilma === "function")
                page.reloadWilma()
            appWindow.activate()
        }
    }
    allowedOrientations: Orientation.All
}
