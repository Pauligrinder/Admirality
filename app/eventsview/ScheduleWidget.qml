import QtQuick 2.6
import Sailfish.Silica 1.0
import Nemo.DBus 2.0

Item {
    id: root

    // Lipstick sizes the widget from implicitHeight and only sets the width.
    // Match Helmsman: report column.height, never childrenRect (that can stay 0).
    width: parent ? parent.width : Screen.width
    implicitWidth: width
    implicitHeight: Math.max(column.height, Theme.itemSizeMedium)
    height: implicitHeight

    property bool active: visible && eventsViewVisible
    property bool appRunning: false
    property string errorText: ""
    property bool loggedIn: false
    property var schedule: []
    property bool weekMode: false
    property int dayOffset: 0
    property int weekOffset: 0
    property var weekModel: emptyWeek()
    property var dayLessonsList: []

    readonly property var palette: [
        "#1565C0", "#2E7D32", "#6A1B9A", "#EF6C00",
        "#00838F", "#C62828", "#4527A0", "#558B2F"
    ]

    function emptyWeek() {
        return { "title": "", "headers": [], "rows": [] }
    }

    function finnish() {
        return String(Qt.locale().name || "").indexOf("fi") === 0
    }

    function textFor(key) {
        var fi = root.finnish()
        if (key === "signIn")
            return fi ? "Kirjaudu Admiralityyn" : "Sign in to Admirality"
        if (key === "noLessons")
            return fi ? "Ei tunteja" : "No lessons"
        if (key === "week")
            return fi ? "Viikko" : "Week"
        if (key === "needsService")
            return fi ? "Wilma-palvelu ei ole käynnissä" : "Wilma service is not running"
        if (key === "unavailable")
            return fi ? "Aikataulua ei saatu" : "Schedule unavailable"
        return ""
    }

    function startOfDay(date) {
        return new Date(date.getFullYear(), date.getMonth(), date.getDate())
    }

    function addDays(date, days) {
        var next = new Date(date.getTime())
        next.setDate(next.getDate() + days)
        return next
    }

    function iso(date) {
        function pad(n) { return (n < 10 ? "0" : "") + n }
        return date.getFullYear() + "-" + pad(date.getMonth() + 1) + "-" + pad(date.getDate())
    }

    function parseIso(text) {
        var parts = String(text || "").split("-")
        if (parts.length < 3)
            return null
        var date = new Date(Number(parts[0]), Number(parts[1]) - 1, Number(parts[2]))
        return isNaN(date.getTime()) ? null : date
    }

    function parseClock(date, text) {
        var bits = String(text || "").split(":")
        if (bits.length < 2)
            bits = String(text || "").split(".")
        if (bits.length < 2)
            return null
        var when = new Date(date.getFullYear(), date.getMonth(), date.getDate(),
                            Number(bits[0]), Number(bits[1]), 0, 0)
        return isNaN(when.getTime()) ? null : when
    }

    function mondayOf(date) {
        var day = date.getDay()
        var delta = day === 0 ? -6 : 1 - day
        return root.addDays(root.startOfDay(date), delta)
    }

    function weekNumber(date) {
        var utc = new Date(Date.UTC(date.getFullYear(), date.getMonth(), date.getDate()))
        var day = utc.getUTCDay() || 7
        utc.setUTCDate(utc.getUTCDate() + 4 - day)
        var yearStart = new Date(Date.UTC(utc.getUTCFullYear(), 0, 1))
        return Math.ceil((((utc - yearStart) / 86400000) + 1) / 7)
    }

    function dayName(date) {
        var names = ["su", "ma", "ti", "ke", "to", "pe", "la"]
        if (root.finnish())
            names = ["su", "ma", "ti", "ke", "to", "pe", "la"]
        else
            names = ["Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"]
        return names[date.getDay()] || ""
    }

    function schoolWeekEnded() {
        var now = new Date()
        var dow = now.getDay()
        if (dow === 0 || dow === 6)
            return true
        var monday = root.mondayOf(now)
        var friday = root.addDays(monday, 5)
        var last = null
        var lessons = root.schedule || []
        for (var i = 0; i < lessons.length; ++i) {
            var lesson = lessons[i]
            var date = root.parseIso(lesson.date)
            if (!date || date < monday || date >= friday)
                continue
            var end = root.parseClock(date, lesson.end || lesson.start)
            if (end && (!last || end > last))
                last = end
        }
        return !!(last && now.getTime() > last.getTime())
    }

    function defaultWeekMonday() {
        var monday = root.mondayOf(root.startOfDay(new Date()))
        if (root.schoolWeekEnded())
            return root.addDays(monday, 7)
        return monday
    }

    function shownDay() {
        return root.addDays(root.startOfDay(new Date()), root.dayOffset)
    }

    function shownWeekMonday() {
        return root.addDays(root.defaultWeekMonday(), root.weekOffset * 7)
    }

    function lessonsOn(date) {
        var key = root.iso(date)
        var out = []
        var lessons = root.schedule || []
        for (var i = 0; i < lessons.length; ++i) {
            if (lessons[i] && lessons[i].date === key)
                out.push(lessons[i])
        }
        out.sort(function(a, b) {
            var as = String(a.start || "")
            var bs = String(b.start || "")
            return as < bs ? -1 : (as > bs ? 1 : 0)
        })
        return out
    }

    function shortName(lesson) {
        var code = String(lesson.subjectCode || "")
        var subject = String(lesson.subject || "")
        if (code.length > 0 && code.length <= 10)
            return code
        if (subject.length <= 10)
            return subject
        return subject.substring(0, 9)
    }

    function colorFor(name) {
        var text = String(name || "")
        var hash = 0
        for (var i = 0; i < text.length; ++i)
            hash = (hash * 33 + text.charCodeAt(i)) >>> 0
        return root.palette[hash % root.palette.length]
    }

    function dayTitle(date) {
        return root.dayName(date) + " " + date.getDate() + "." + (date.getMonth() + 1) + "."
    }

    function buildWeekModel() {
        var monday = root.shownWeekMonday()
        var days = []
        var headers = []
        var d
        for (d = 0; d < 5; ++d) {
            var date = root.addDays(monday, d)
            var key = root.iso(date)
            days.push(key)
            headers.push({
                             "label": root.dayName(date),
                             "date": date.getDate() + "." + (date.getMonth() + 1) + ".",
                             "today": key === root.iso(root.startOfDay(new Date()))
                         })
        }
        var slots = {}
        var order = []
        var lessons = root.schedule || []
        var i
        for (i = 0; i < lessons.length; ++i) {
            var lesson = lessons[i]
            var index = days.indexOf(String(lesson.date || ""))
            if (index < 0)
                continue
            var start = String(lesson.start || "")
            if (!slots[start]) {
                slots[start] = {
                    "start": start,
                    "cells": [[], [], [], [], []],
                    "count": 1
                }
                order.push(start)
            }
            slots[start].cells[index].push(lesson)
            var maxCount = 1
            var c
            for (c = 0; c < 5; ++c) {
                if (slots[start].cells[c].length > maxCount)
                    maxCount = slots[start].cells[c].length
            }
            slots[start].count = maxCount
        }
        order.sort()
        var rows = []
        for (i = 0; i < order.length; ++i)
            rows.push(slots[order[i]])
        return {
            "title": root.textFor("week") + " " + root.weekNumber(monday),
            "headers": headers,
            "rows": rows
        }
    }

    function rebuildWeek() {
        try {
            root.weekModel = root.buildWeekModel()
        } catch (e) {
            root.weekModel = root.emptyWeek()
        }
    }

    function refreshDayLessons() {
        root.dayLessonsList = root.lessonsOn(root.shownDay())
    }

    function applyPayload(payload) {
        root.appRunning = true
        root.errorText = ""
        if (!payload) {
            root.loggedIn = false
            root.schedule = []
            root.refreshDayLessons()
            return
        }
        try {
            var state = JSON.parse(payload)
            root.loggedIn = !!state.loggedIn
            root.schedule = state.schedule || []
            root.refreshDayLessons()
            if (root.weekMode)
                root.rebuildWeek()
        } catch (e) {
            root.loggedIn = false
            root.schedule = []
            root.refreshDayLessons()
            root.errorText = root.textFor("unavailable")
        }
    }

    function fetchState() {
        if (!root.active)
            return
        wilma.call("GetState", [],
                   function(result) { root.applyPayload(result) },
                   function() {
                       root.appRunning = false
                       root.loggedIn = false
                       root.schedule = []
                       root.errorText = root.textFor("needsService")
                   })
    }

    function refresh() {
        if (!root.active)
            return
        root.fetchState()
    }

    function reload() { refresh() }
    function save() {}

    Component.onCompleted: {
        root.refreshDayLessons()
        if (active)
            refresh()
    }
    onActiveChanged: if (active) refresh()
    onDayOffsetChanged: root.refreshDayLessons()
    onWeekOffsetChanged: if (root.weekMode) root.rebuildWeek()

    Timer {
        interval: 30000
        repeat: true
        running: root.active
        onTriggered: root.fetchState()
    }

    DBusInterface {
        id: wilma
        service: "org.admirality.harbour-admirality"
        path: "/wilma"
        iface: "org.admirality.Wilma"
        signalsEnabled: root.active
        function stateChanged() {
            root.fetchState()
        }
    }

    Column {
        id: column
        width: parent.width
        spacing: Theme.paddingSmall

        Row {
            width: parent.width
            height: Theme.itemSizeSmall

            MouseArea {
                width: Theme.itemSizeSmall
                height: parent.height
                onClicked: {
                    if (root.weekMode)
                        root.weekOffset -= 1
                    else
                        root.dayOffset -= 1
                }
                Image {
                    anchors.centerIn: parent
                    width: Theme.iconSizeMedium
                    height: Theme.iconSizeMedium
                    source: "image://theme/icon-m-left"
                }
            }

            Label {
                width: parent.width - Theme.itemSizeSmall * (root.weekMode ? 4 : 3)
                height: parent.height
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                truncationMode: TruncationMode.Fade
                color: Theme.highlightColor
                font.pixelSize: Theme.fontSizeMedium
                font.family: Theme.fontFamilyHeading
                text: root.weekMode
                      ? (root.weekModel.title || root.textFor("week"))
                      : root.dayTitle(root.shownDay())
            }

            MouseArea {
                width: Theme.itemSizeSmall
                height: parent.height
                onClicked: {
                    if (root.weekMode)
                        root.weekOffset += 1
                    else
                        root.dayOffset += 1
                }
                Image {
                    anchors.centerIn: parent
                    width: Theme.iconSizeMedium
                    height: Theme.iconSizeMedium
                    source: "image://theme/icon-m-right"
                }
            }

            MouseArea {
                width: root.weekMode ? Theme.itemSizeSmall : 0
                height: parent.height
                visible: root.weekMode
                onClicked: root.weekOffset = 0
                Image {
                    anchors.centerIn: parent
                    width: Theme.iconSizeMedium
                    height: Theme.iconSizeMedium
                    visible: parent.visible
                    source: "image://theme/icon-m-home"
                }
            }

            MouseArea {
                width: Theme.itemSizeSmall
                height: parent.height
                onClicked: {
                    root.weekMode = !root.weekMode
                    if (root.weekMode)
                        root.rebuildWeek()
                }
                Image {
                    anchors.centerIn: parent
                    width: Theme.iconSizeMedium
                    height: Theme.iconSizeMedium
                    source: root.weekMode
                            ? "image://theme/icon-m-back"
                            : "image://theme/icon-m-calendar"
                }
            }
        }

        Label {
            x: Theme.horizontalPageMargin
            width: parent.width - 2 * x
            visible: {
                if (!root.loggedIn)
                    return true
                if (root.weekMode)
                    return !(root.weekModel.rows && root.weekModel.rows.length)
                return root.dayLessonsList.length === 0
            }
            wrapMode: Text.Wrap
            color: Theme.secondaryColor
            font.pixelSize: Theme.fontSizeSmall
            text: {
                if (!root.appRunning)
                    return root.errorText.length > 0 ? root.errorText : root.textFor("needsService")
                if (root.errorText.length > 0)
                    return root.errorText
                return root.loggedIn ? root.textFor("noLessons") : root.textFor("signIn")
            }
        }

        Column {
            width: parent.width
            visible: !root.weekMode && root.loggedIn && root.dayLessonsList.length > 0
            spacing: 0

            Repeater {
                model: root.dayLessonsList
                delegate: Item {
                    width: column.width
                    height: dayCol.height + Theme.paddingMedium

                    Column {
                        id: dayCol
                        y: Theme.paddingMedium / 2
                        x: Theme.horizontalPageMargin
                        width: parent.width - 2 * x

                        Label {
                            width: parent.width
                            truncationMode: TruncationMode.Fade
                            color: Theme.primaryColor
                            text: modelData.subject || modelData.subjectCode || ""
                        }

                        Label {
                            width: parent.width
                            truncationMode: TruncationMode.Fade
                            color: Theme.secondaryColor
                            font.pixelSize: Theme.fontSizeExtraSmall
                            text: {
                                var time = modelData.start && modelData.end
                                           ? modelData.start + "–" + modelData.end
                                           : (modelData.start || "")
                                var extra = []
                                if (modelData.teacher)
                                    extra.push(modelData.teacher)
                                if (modelData.subjectCode && modelData.subjectCode !== modelData.subject)
                                    extra.push(modelData.subjectCode)
                                return extra.length ? time + "  " + extra.join(" · ") : time
                            }
                        }
                    }
                }
            }
        }

        Column {
            id: weekGrid
            width: parent.width - 2 * Theme.paddingMedium
            x: Theme.paddingMedium
            spacing: Theme.paddingSmall
            visible: root.weekMode && root.weekModel.rows && root.weekModel.rows.length > 0

            property int timeWidth: Math.max(Theme.itemSizeSmall, Math.round(width * 0.16))
            property int dayWidth: Math.max(1, Math.floor((width - timeWidth) / 5))

            Row {
                width: weekGrid.width
                height: Theme.fontSizeSmall * 3

                Item { width: weekGrid.timeWidth; height: 1 }

                Repeater {
                    model: root.weekModel.headers
                    delegate: Column {
                        width: weekGrid.dayWidth
                        Label {
                            width: parent.width
                            horizontalAlignment: Text.AlignHCenter
                            font.pixelSize: Theme.fontSizeExtraSmall
                            font.bold: modelData.today
                            color: modelData.today ? Theme.highlightColor : Theme.primaryColor
                            text: modelData.label
                        }
                        Label {
                            width: parent.width
                            horizontalAlignment: Text.AlignHCenter
                            font.pixelSize: Theme.fontSizeExtraSmall
                            color: modelData.today ? Theme.secondaryHighlightColor : Theme.secondaryColor
                            text: modelData.date
                        }
                    }
                }
            }

            Repeater {
                model: root.weekModel.rows
                delegate: Row {
                    width: weekGrid.width
                    height: Math.max(Theme.itemSizeSmall,
                                     (modelData.count || 1) * (Theme.fontSizeExtraSmall + Theme.paddingMedium)
                                     + Theme.paddingSmall)

                    Label {
                        width: weekGrid.timeWidth
                        height: parent.height
                        verticalAlignment: Text.AlignVCenter
                        font.pixelSize: Theme.fontSizeExtraSmall
                        color: Theme.secondaryColor
                        text: modelData.start || ""
                    }

                    Repeater {
                        model: modelData.cells
                        delegate: Item {
                            width: weekGrid.dayWidth
                            height: parent.height

                            Column {
                                id: slotCol
                                width: parent.width - Theme.paddingSmall
                                anchors.centerIn: parent
                                spacing: 2

                                Repeater {
                                    model: modelData
                                    delegate: Rectangle {
                                        width: slotCol.width
                                        height: Theme.fontSizeExtraSmall + Theme.paddingSmall
                                        radius: Theme.paddingSmall
                                        color: root.colorFor(modelData.subjectCode || modelData.subject)

                                        Label {
                                            anchors.centerIn: parent
                                            width: parent.width - Theme.paddingSmall
                                            horizontalAlignment: Text.AlignHCenter
                                            elide: Text.ElideRight
                                            font.pixelSize: Theme.fontSizeExtraSmall
                                            font.bold: true
                                            color: "white"
                                            text: root.shortName(modelData)
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        Item { width: 1; height: Theme.paddingSmall }
    }
}
