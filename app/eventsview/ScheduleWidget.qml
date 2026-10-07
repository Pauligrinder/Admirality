import QtQuick 2.6
import Sailfish.Silica 1.0
import Nemo.DBus 2.0

Item {
    id: root

    // Lipstick sizes the widget from implicitHeight and only sets the width.
    width: parent ? parent.width : Screen.width
    implicitWidth: width
    // childrenRect stays non-zero when a child binding loop would report
    // column.height as 0, which otherwise makes lipstick drop the widget.
    implicitHeight: Math.max(column.childrenRect.height, Theme.itemSizeSmall)
    height: implicitHeight

    property bool active: visible && eventsViewVisible
    property bool appRunning: false
    property string errorText: ""
    property bool loggedIn: false
    property var schedule: []
    property bool weekMode: false
    property int dayOffset: 0
    property int weekOffset: 0

    readonly property var palette: [
        "#1565C0", "#2E7D32", "#6A1B9A", "#EF6C00",
        "#00838F", "#C62828", "#4527A0", "#558B2F"
    ]

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
        var qtDay = date.getDay() === 0 ? 7 : date.getDay()
        var name = ""
        try {
            name = Qt.locale().dayName(qtDay, Locale.ShortFormat)
        } catch (e) {
            name = ""
        }
        if (!name.length)
            name = Qt.locale().dayName(qtDay)
        return name + " " + date.getDate() + "." + (date.getMonth() + 1) + "."
    }

    function emptyWeek() {
        return { "title": "", "headers": [], "rows": [] }
    }

    function buildWeekModel() {
        var monday = root.shownWeekMonday()
        var days = []
        var headers = []
        for (var d = 0; d < 5; ++d) {
            var date = root.addDays(monday, d)
            var key = root.iso(date)
            var qtDay = date.getDay() === 0 ? 7 : date.getDay()
            var label = ""
            try {
                label = Qt.locale().dayName(qtDay, Locale.ShortFormat)
            } catch (e) {
                label = ""
            }
            if (!label.length)
                label = Qt.locale().dayName(qtDay)
            days.push(key)
            headers.push({
                             "label": label,
                             "date": date.getDate() + "." + (date.getMonth() + 1) + ".",
                             "today": key === root.iso(root.startOfDay(new Date())),
                             "iso": key
                         })
        }
        var slots = {}
        var order = []
        var lessons = root.schedule || []
        for (var i = 0; i < lessons.length; ++i) {
            var lesson = lessons[i]
            var index = days.indexOf(String(lesson.date || ""))
            if (index < 0)
                continue
            var start = String(lesson.start || "")
            if (!slots[start]) {
                slots[start] = {
                    "start": start,
                    "end": String(lesson.end || ""),
                    "cells": [[], [], [], [], []],
                    "count": 1
                }
                order.push(start)
            }
            slots[start].cells[index].push(lesson)
            var maxCount = 1
            for (var c = 0; c < 5; ++c) {
                if (slots[start].cells[c].length > maxCount)
                    maxCount = slots[start].cells[c].length
            }
            slots[start].count = maxCount
        }
        order.sort()
        var rows = []
        for (var r = 0; r < order.length; ++r)
            rows.push(slots[order[r]])
        return {
            "title": root.textFor("week") + " " + root.weekNumber(monday),
            "headers": headers,
            "rows": rows
        }
    }

    function rebuildWeek() {
        var built = root.emptyWeek()
        try {
            built = root.buildWeekModel()
        } catch (e) {
            built = root.emptyWeek()
        }
        root.weekModel = built
    }

    readonly property var dayLessons: root.lessonsOn(root.shownDay())
    property var weekModel: ({ "title": "", "headers": [], "rows": [] })

    onScheduleChanged: if (root.weekMode) root.rebuildWeek()
    onWeekOffsetChanged: if (root.weekMode) root.rebuildWeek()
    onWeekModeChanged: if (root.weekMode) root.rebuildWeek()

    function applyPayload(payload) {
        root.appRunning = true
        root.errorText = ""
        if (!payload) {
            root.loggedIn = false
            root.schedule = []
            return
        }
        try {
            var state = JSON.parse(payload)
            root.loggedIn = !!state.loggedIn
            root.schedule = state.schedule || []
        } catch (e) {
            root.loggedIn = false
            root.schedule = []
            root.errorText = root.textFor("unavailable")
        }
    }

    function fetchState() {
        // Replacing the model while the events view is off screen is wasted work.
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

    Component.onCompleted: if (active) refresh()
    onActiveChanged: if (active) refresh()

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
        // Nemo.DBus maps StateChanged to a lowercase-initial handler, same as Helmsman.
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

            IconButton {
                width: Theme.itemSizeSmall
                height: Theme.itemSizeSmall
                icon.source: "image://theme/icon-m-left"
                onClicked: {
                    if (root.weekMode)
                        root.weekOffset -= 1
                    else
                        root.dayOffset -= 1
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
                text: root.weekMode ? root.weekModel.title : root.dayTitle(root.shownDay())
            }

            IconButton {
                width: Theme.itemSizeSmall
                height: Theme.itemSizeSmall
                icon.source: "image://theme/icon-m-right"
                onClicked: {
                    if (root.weekMode)
                        root.weekOffset += 1
                    else
                        root.dayOffset += 1
                }
            }

            IconButton {
                width: Theme.itemSizeSmall
                height: Theme.itemSizeSmall
                visible: root.weekMode
                icon.source: "image://theme/icon-m-home"
                onClicked: root.weekOffset = 0
            }

            IconButton {
                width: Theme.itemSizeSmall
                height: Theme.itemSizeSmall
                icon.source: root.weekMode
                             ? "image://theme/icon-m-back"
                             : "image://theme/icon-m-calendar"
                onClicked: {
                    if (!root.weekMode)
                        root.rebuildWeek()
                    root.weekMode = !root.weekMode
                }
            }
        }

        Label {
            x: Theme.horizontalPageMargin
            width: parent.width - 2 * x
            visible: !root.loggedIn || (!root.weekMode && root.dayLessons.length === 0)
                     || (root.weekMode && root.weekModel.rows.length === 0)
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
            visible: !root.weekMode && root.dayLessons.length > 0

            Repeater {
                model: root.dayLessons
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

        Loader {
            id: weekLoader
            width: parent.width
            active: root.weekMode && root.weekModel.rows && root.weekModel.rows.length > 0
            height: active && item ? item.implicitHeight : 0
            sourceComponent: Component {
                Column {
                    id: grid
                    width: weekLoader.width - 2 * Theme.paddingMedium
                    x: Theme.paddingMedium
                    spacing: Theme.paddingSmall
                    implicitHeight: childrenRect.height

            property int timeWidth: Math.max(Theme.itemSizeSmall, Math.round(width * 0.16))
            property int dayWidth: Math.floor((width - timeWidth) / 5)

            Row {
                width: grid.width
                height: Theme.fontSizeSmall * 3
                spacing: 0

                Item { width: grid.timeWidth; height: 1 }

                Repeater {
                    model: root.weekModel.headers
                    delegate: Column {
                        id: headerCol
                        width: grid.dayWidth
                        spacing: 0

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
                    width: grid.width
                    height: Math.max(Theme.itemSizeSmall,
                                     (modelData.count || 1) * (Theme.fontSizeExtraSmall + Theme.paddingMedium)
                                     + Theme.paddingSmall)
                    spacing: 0

                    Label {
                        width: grid.timeWidth
                        height: parent.height
                        verticalAlignment: Text.AlignVCenter
                        font.pixelSize: Theme.fontSizeExtraSmall
                        color: Theme.secondaryColor
                        text: modelData.start || ""
                    }

                    Repeater {
                        model: modelData.cells
                        delegate: Item {
                            width: grid.dayWidth
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
                                        height: codeLabel.implicitHeight + Theme.paddingSmall
                                        radius: Theme.paddingSmall
                                        color: root.colorFor(modelData.subjectCode || modelData.subject)

                                        Label {
                                            id: codeLabel
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
            }
        }

        Item { width: 1; height: Theme.paddingSmall }
    }
}
