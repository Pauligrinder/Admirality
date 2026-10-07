import QtQuick 2.6
import Sailfish.Silica 1.0

Item {
    id: page
    objectName: "HomePage"
    anchors.fill: parent

    function reloadWilma() {
        wilmaClient.refreshHome()
    }

    function preview(list, limit) {
        var all = list || []
        var out = []
        var n = all.length < limit ? all.length : limit
        for (var i = 0; i < n; ++i)
            out.push(all[i])
        return out
    }

    function openMessages() {
        var p = pageStack.currentPage
        if (p && p.objectName === "MainPage" && typeof p.showTab === "function")
            p.showTab(1)
        else
            pageStack.push(Qt.resolvedUrl("MessagesPage.qml"))
    }

    function openSchedule() {
        var p = pageStack.currentPage
        if (p && p.objectName === "MainPage" && typeof p.showTab === "function")
            p.showTab(2)
        else
            pageStack.push(Qt.resolvedUrl("SchedulePage.qml"))
    }

    function openNews() {
        var p = pageStack.currentPage
        if (p && p.objectName === "MainPage" && typeof p.showTab === "function")
            p.showTab(4)
        else
            pageStack.push(Qt.resolvedUrl("NewsPage.qml"))
    }

    function openNotes() {
        var p = pageStack.currentPage
        if (p && p.objectName === "MainPage" && typeof p.showTab === "function")
            p.showTab(3)
        else
            pageStack.push(Qt.resolvedUrl("NotesPage.qml"))
    }

    function reveal(which) {
        var target = null
        if (which === "exams")
            target = examsHeader
        else if (which === "homework")
            target = homeworkHeader
        else if (which === "grades")
            target = gradesHeader
        if (!target)
            return
        Qt.callLater(function() {
            var maxY = Math.max(0, flick.contentHeight - flick.height)
            flick.contentY = Math.max(0, Math.min(target.y, maxY))
        })
    }

    function openWilmaSite() {
        pageStack.push(Qt.resolvedUrl("WilmaPage.qml"))
    }

    function openMessage(item) {
        pageStack.push(Qt.resolvedUrl("MessagePage.qml"), {
                           messageId: item.id,
                           subject: item.subject || "",
                           sender: item.sender || ""
                       })
    }

    function openNewsItem(item) {
        pageStack.push(Qt.resolvedUrl("NewsDetailPage.qml"), {
                           newsId: item.id,
                           title: item.title || ""
                       })
    }

    Component.onCompleted: {
        if (wilmaClient.loggedIn && wilmaClient.roleId.length > 0)
            wilmaClient.refreshHome()
        wilmaClient.acknowledgeGrades()
        wilmaClient.acknowledgeHomework()
        wilmaClient.acknowledgeExams()
    }

    SilicaFlickable {
        id: flick
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        VerticalScrollDecorator {}

        PullDownMenu {
            busy: wilmaClient.refreshing
            MenuItem {
                text: qsTr("Settings")
                onClicked: pageStack.push(Qt.resolvedUrl("SettingsPage.qml"))
            }
            MenuItem {
                text: qsTr("Open Wilma site")
                onClicked: page.openWilmaSite()
            }
            MenuItem {
                visible: wilmaClient.roles.length > 1
                text: qsTr("Change user")
                onClicked: pageStack.push(Qt.resolvedUrl("RolePickerPage.qml"))
            }
            MenuItem {
                text: qsTr("Refresh")
                onClicked: page.reloadWilma()
            }
        }

        Column {
            id: column
            width: parent.width
            spacing: Theme.paddingMedium

            PageHeader {
                title: wilmaClient.roleName.length > 0
                       ? wilmaClient.roleName
                       : (wilmaClient.displayName.length > 0
                          ? wilmaClient.displayName
                          : qsTr("Admirality"))
                description: wilmaClient.schoolName.length > 0
                             ? wilmaClient.schoolName
                             : wilmaClient.schoolHost
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeExtraSmall
                text: {
                    if (wilmaClient.unreadCount === 1)
                        return qsTr("1 unread message")
                    if (wilmaClient.unreadCount > 1)
                        return qsTr("%1 unread messages").arg(wilmaClient.unreadCount)
                    return ""
                }
                visible: text.length > 0
            }

            SectionHeader { text: qsTr("Today") }

            Repeater {
                model: wilmaClient.todaySchedule
                delegate: BackgroundItem {
                    width: column.width
                    height: todayCol.height + Theme.paddingMedium * 2
                    enabled: false

                    Column {
                        id: todayCol
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
                            text: modelData.subject
                            color: Theme.primaryColor
                        }

                        Label {
                            width: parent.width
                            wrapMode: Text.Wrap
                            color: Theme.secondaryColor
                            font.pixelSize: Theme.fontSizeExtraSmall
                            text: {
                                var time = modelData.start && modelData.end
                                          ? modelData.start + "–" + modelData.end
                                          : (modelData.start || "")
                                var extra = []
                                if (modelData.teacher)
                                    extra.push(modelData.teacher)
                                if (modelData.subjectCode)
                                    extra.push(modelData.subjectCode)
                                return extra.length ? time + " · " + extra.join(" · ") : time
                            }
                        }
                    }
                }
            }

            Label {
                visible: wilmaClient.todaySchedule.length === 0
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeSmall
                text: wilmaClient.refreshing
                      ? qsTr("Loading today’s lessons…")
                      : qsTr("No lessons today")
            }

            BackgroundItem {
                width: parent.width
                visible: wilmaClient.schedule.length > 0
                onClicked: page.openSchedule()

                Label {
                    anchors {
                        left: parent.left
                        right: parent.right
                        verticalCenter: parent.verticalCenter
                        leftMargin: Theme.horizontalPageMargin
                        rightMargin: Theme.horizontalPageMargin
                    }
                    color: Theme.highlightColor
                    text: qsTr("Full schedule")
                }
            }

            SectionHeader { text: qsTr("Lesson notes") }

            Repeater {
                model: page.preview(wilmaClient.lessonNotes, 6)
                delegate: BackgroundItem {
                    width: column.width
                    height: noteCol.height + Theme.paddingMedium * 2
                    enabled: false

                    Rectangle {
                        id: noteDot
                        anchors {
                            left: parent.left
                            leftMargin: Theme.horizontalPageMargin
                            verticalCenter: parent.verticalCenter
                        }
                        width: Theme.paddingMedium
                        height: width
                        radius: width / 2
                        color: modelData.dotColor ? modelData.dotColor : "#9e9e9e"
                    }

                    Column {
                        id: noteCol
                        anchors {
                            left: noteDot.right
                            right: parent.right
                            verticalCenter: parent.verticalCenter
                            leftMargin: Theme.paddingMedium
                            rightMargin: Theme.horizontalPageMargin
                        }

                        Label {
                            width: parent.width
                            wrapMode: Text.Wrap
                            text: modelData.typeLabel || qsTr("Lesson note")
                            font.bold: modelData.needsAction
                            color: modelData.needsAction ? Theme.highlightColor : Theme.primaryColor
                        }

                        Label {
                            width: parent.width
                            wrapMode: Text.Wrap
                            color: Theme.secondaryColor
                            font.pixelSize: Theme.fontSizeExtraSmall
                            text: {
                                var parts = []
                                if (modelData.dateLabel)
                                    parts.push(modelData.dateLabel)
                                if (modelData.start && modelData.end)
                                    parts.push(modelData.start + "–" + modelData.end)
                                else if (modelData.start)
                                    parts.push(modelData.start)
                                if (modelData.subject)
                                    parts.push(modelData.subject)
                                if (modelData.teacher)
                                    parts.push(modelData.teacher)
                                return parts.join(" · ")
                            }
                        }
                    }
                }
            }

            Label {
                visible: wilmaClient.lessonNotes.length === 0
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeSmall
                text: wilmaClient.refreshing
                      ? qsTr("Loading lesson notes…")
                      : qsTr("No lesson notes")
            }

            BackgroundItem {
                width: parent.width
                visible: wilmaClient.lessonNotes.length > 0
                onClicked: page.openNotes()

                Label {
                    anchors {
                        left: parent.left
                        right: parent.right
                        verticalCenter: parent.verticalCenter
                        leftMargin: Theme.horizontalPageMargin
                        rightMargin: Theme.horizontalPageMargin
                    }
                    color: Theme.highlightColor
                    text: qsTr("All lesson notes")
                }
            }

            SectionHeader { text: qsTr("Messages") }

            Repeater {
                model: page.preview(wilmaClient.messages, 6)
                delegate: ListItem {
                    width: column.width
                    contentHeight: msgCol.height + Theme.paddingMedium * 2
                    onClicked: page.openMessage(modelData)

                    Column {
                        id: msgCol
                        anchors {
                            left: parent.left
                            right: parent.right
                            verticalCenter: parent.verticalCenter
                            leftMargin: Theme.horizontalPageMargin
                            rightMargin: Theme.horizontalPageMargin
                        }

                        Label {
                            width: parent.width
                            truncationMode: TruncationMode.Fade
                            text: modelData.subject || qsTr("Message")
                            font.bold: modelData.unread
                            color: modelData.unread ? Theme.highlightColor : Theme.primaryColor
                        }

                        Label {
                            width: parent.width
                            truncationMode: TruncationMode.Fade
                            color: Theme.secondaryColor
                            font.pixelSize: Theme.fontSizeExtraSmall
                            text: {
                                var parts = []
                                if (modelData.sender)
                                    parts.push(modelData.sender)
                                if (modelData.time)
                                    parts.push(modelData.time)
                                return parts.join(" · ")
                            }
                        }
                    }
                }
            }

            Label {
                visible: wilmaClient.messages.length === 0
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeSmall
                text: wilmaClient.refreshing
                      ? qsTr("Loading messages…")
                      : qsTr("No messages")
            }

            BackgroundItem {
                width: parent.width
                visible: wilmaClient.messages.length > 0
                onClicked: page.openMessages()

                Label {
                    anchors {
                        left: parent.left
                        right: parent.right
                        verticalCenter: parent.verticalCenter
                        leftMargin: Theme.horizontalPageMargin
                        rightMargin: Theme.horizontalPageMargin
                    }
                    color: Theme.highlightColor
                    text: qsTr("All messages")
                }
            }

            SectionHeader { text: qsTr("News") }

            Repeater {
                model: page.preview(wilmaClient.news, 5)
                delegate: ListItem {
                    width: column.width
                    contentHeight: newsCol.height + Theme.paddingMedium * 2
                    onClicked: page.openNewsItem(modelData)

                    Column {
                        id: newsCol
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
                            color: Theme.primaryColor
                        }

                        Label {
                            width: parent.width
                            truncationMode: TruncationMode.Fade
                            visible: (modelData.published || modelData.author || "").length > 0
                            color: Theme.secondaryColor
                            font.pixelSize: Theme.fontSizeExtraSmall
                            text: {
                                var parts = []
                                if (modelData.published)
                                    parts.push(modelData.published)
                                if (modelData.author)
                                    parts.push(modelData.author)
                                return parts.join(" · ")
                            }
                        }
                    }
                }
            }

            Label {
                visible: wilmaClient.news.length === 0
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeSmall
                text: wilmaClient.refreshing
                      ? qsTr("Loading news…")
                      : qsTr("No news")
            }

            BackgroundItem {
                width: parent.width
                visible: wilmaClient.news.length > 0
                onClicked: page.openNews()

                Label {
                    anchors {
                        left: parent.left
                        right: parent.right
                        verticalCenter: parent.verticalCenter
                        leftMargin: Theme.horizontalPageMargin
                        rightMargin: Theme.horizontalPageMargin
                    }
                    color: Theme.highlightColor
                    text: qsTr("All news")
                }
            }

            SectionHeader {
                id: examsHeader
                text: qsTr("Exams")
            }

            Repeater {
                model: page.preview(wilmaClient.exams, 6)
                delegate: BackgroundItem {
                    width: column.width
                    height: examCol.height + Theme.paddingMedium * 2
                    enabled: false

                    Column {
                        id: examCol
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
                            text: modelData.name || modelData.subject || qsTr("Exam")
                            color: Theme.primaryColor
                        }

                        Label {
                            width: parent.width
                            wrapMode: Text.Wrap
                            color: Theme.secondaryColor
                            font.pixelSize: Theme.fontSizeExtraSmall
                            text: {
                                var parts = []
                                if (modelData.dateLabel)
                                    parts.push(modelData.dateLabel)
                                if (modelData.subject && modelData.name)
                                    parts.push(modelData.subject)
                                if (modelData.topic)
                                    parts.push(modelData.topic)
                                return parts.join(" · ")
                            }
                        }
                    }
                }
            }

            Label {
                visible: wilmaClient.exams.length === 0
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeSmall
                text: qsTr("No upcoming exams")
            }

            SectionHeader {
                id: homeworkHeader
                text: qsTr("Homework")
            }

            Repeater {
                model: page.preview(wilmaClient.homework, 6)
                delegate: BackgroundItem {
                    width: column.width
                    height: hwCol.height + Theme.paddingMedium * 2
                    enabled: false

                    Column {
                        id: hwCol
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
                            text: modelData.subject || qsTr("Homework")
                            color: Theme.primaryColor
                        }

                        Label {
                            width: parent.width
                            wrapMode: Text.Wrap
                            color: Theme.secondaryColor
                            font.pixelSize: Theme.fontSizeSmall
                            text: modelData.homework
                        }

                        Label {
                            width: parent.width
                            visible: (modelData.dateLabel || "").length > 0
                            color: Theme.secondaryHighlightColor
                            font.pixelSize: Theme.fontSizeExtraSmall
                            text: modelData.dateLabel
                        }
                    }
                }
            }

            Label {
                visible: wilmaClient.homework.length === 0
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeSmall
                text: qsTr("No homework")
            }

            SectionHeader {
                id: gradesHeader
                visible: wilmaClient.grades.length > 0
                text: qsTr("Recent grades")
            }

            Repeater {
                model: page.preview(wilmaClient.grades, 5)
                delegate: BackgroundItem {
                    width: column.width
                    height: gradeCol.height + Theme.paddingMedium * 2
                    enabled: false

                    Column {
                        id: gradeCol
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
                            text: (modelData.subject || modelData.name || qsTr("Grade"))
                                  + "  " + modelData.grade
                            color: Theme.primaryColor
                        }

                        Label {
                            width: parent.width
                            visible: (modelData.dateLabel || modelData.name || "").length > 0
                            color: Theme.secondaryColor
                            font.pixelSize: Theme.fontSizeExtraSmall
                            text: {
                                var parts = []
                                if (modelData.dateLabel)
                                    parts.push(modelData.dateLabel)
                                if (modelData.name && modelData.subject)
                                    parts.push(modelData.name)
                                return parts.join(" · ")
                            }
                        }
                    }
                }
            }

            Item { width: 1; height: Theme.paddingLarge }
        }
    }
}
