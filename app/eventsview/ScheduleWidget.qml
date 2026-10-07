import QtQuick 2.6
import Sailfish.Silica 1.0
import Nemo.DBus 2.0

Item {
    id: root

    readonly property color coverBlue: "#0B365C"
    readonly property var palette: [
        "#1565C0", "#2E7D32", "#6A1B9A", "#EF6C00",
        "#00838F", "#C62828", "#4527A0", "#558B2F"
    ]

    width: parent ? parent.width : Screen.width
    implicitWidth: width
    height: Math.max(column.height, Theme.itemSizeMedium)
    implicitHeight: height

    property bool active: visible && eventsViewVisible
    property bool appRunning: false
    property bool loggedIn: false
    property int weekOffset: 0
    property string statusLine: ""
    property var schedule: []
    property var weekHeaders: []
    property var weekRows: []
    property string weekTitle: ""

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
        var names = root.finnish()
                ? ["su", "ma", "ti", "ke", "to", "pe", "la"]
                : ["Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"]
        return names[date.getDay()] || ""
    }

    function shownWeekMonday() {
        return root.addDays(root.mondayOf(root.startOfDay(new Date())), root.weekOffset * 7)
    }

    function isMeal(lesson) {
        var s = String(lesson.subject || lesson.subjectCode || "").toLowerCase()
        return s.indexOf("ruokailu") >= 0
                || s.indexOf("ruoka") >= 0
                || s.indexOf("lunch") >= 0
                || s.indexOf("meal") >= 0
    }

    function shortName(lesson) {
        var raw = String(lesson.subjectCode || lesson.subject || "")
        raw = raw.replace(/^\s*-\s*/, "").replace(/\s+/g, " ").trim()
        var letters = raw.replace(/[^A-Za-zÄÖÅäöå]/g, "")
        if (letters.length >= 2)
            return letters.substring(0, 2)
        if (raw.length >= 2)
            return raw.substring(0, 2)
        return raw
    }

    function colorFor(name) {
        var text = String(name || "")
        var hash = 0
        for (var i = 0; i < text.length; ++i)
            hash = (hash * 33 + text.charCodeAt(i)) >>> 0
        return root.palette[hash % root.palette.length]
    }

    function emptyCell() {
        return { "label": "", "color": "transparent", "meal": false }
    }

    function cellFor(lesson) {
        if (!lesson)
            return root.emptyCell()
        if (root.isMeal(lesson)) {
            return {
                "label": "🍽",
                "color": "#455A64",
                "meal": true
            }
        }
        var label = root.shortName(lesson)
        return {
            "label": label,
            "color": root.colorFor(lesson.subjectCode || lesson.subject || label),
            "meal": false
        }
    }

    function rebuildWeek() {
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
                             "iso": key,
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
                slots[start] = [
                            root.emptyCell(), root.emptyCell(), root.emptyCell(),
                            root.emptyCell(), root.emptyCell()
                        ]
                order.push(start)
            }
            if (!slots[start][index].label.length)
                slots[start][index] = root.cellFor(lesson)
        }
        order.sort()

        var rows = []
        for (i = 0; i < order.length; ++i)
            rows.push({ "start": order[i], "cells": slots[order[i]] })

        root.weekTitle = root.textFor("week") + " " + root.weekNumber(monday)
        root.weekHeaders = headers
        root.weekRows = rows
        if (!root.loggedIn)
            root.statusLine = root.textFor("signIn")
        else if (!rows.length)
            root.statusLine = root.textFor("noLessons")
        else
            root.statusLine = ""
    }

    function applyPayload(payload) {
        root.appRunning = true
        if (!payload) {
            root.loggedIn = false
            root.schedule = []
            root.rebuildWeek()
            root.statusLine = root.textFor("signIn")
            return
        }
        try {
            var state = JSON.parse(payload)
            root.loggedIn = !!state.loggedIn
            root.schedule = state.schedule || []
            root.rebuildWeek()
        } catch (e) {
            root.appRunning = false
            root.loggedIn = false
            root.schedule = []
            root.rebuildWeek()
            root.statusLine = root.textFor("unavailable")
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
                       root.rebuildWeek()
                       root.statusLine = root.textFor("needsService")
                   })
    }

    function refresh() { if (root.active) root.fetchState() }
    function reload() { refresh() }
    function save() {}

    Component.onCompleted: if (active) refresh()
    onActiveChanged: if (active) refresh()
    onWeekOffsetChanged: root.rebuildWeek()

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
        function stateChanged() { root.fetchState() }
    }

    Column {
        id: column
        width: parent.width
        spacing: Theme.paddingSmall

        Item {
            x: Theme.horizontalPageMargin
            width: parent.width - 2 * x
            height: Math.max(cardColumn.y + cardColumn.height + Theme.paddingMedium,
                             Theme.itemSizeLarge + Theme.paddingMedium)

            Rectangle {
                anchors.fill: parent
                radius: Theme.paddingSmall
                color: root.coverBlue
                opacity: root.appRunning ? 1.0 : 0.55
                clip: true

                Image {
                    anchors.left: parent.left
                    anchors.leftMargin: Theme.paddingMedium
                    anchors.top: parent.top
                    anchors.topMargin: Theme.paddingSmall
                    width: Theme.iconSizeLarge * 1.6
                    height: width
                    sourceSize.width: width
                    sourceSize.height: height
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    asynchronous: true
                    opacity: 0.22
                    source: "image://theme/icon-m-date?#FFFFFF"
                    z: 0
                }
            }

            Column {
                id: cardColumn
                z: 1
                x: Theme.paddingMedium
                y: Theme.paddingMedium
                width: parent.width - 2 * x
                spacing: Theme.paddingSmall

                Row {
                    width: parent.width
                    height: Theme.itemSizeSmall

                    BackgroundItem {
                        width: Theme.itemSizeSmall
                        height: parent.height
                        onClicked: root.weekOffset -= 1
                        Image {
                            anchors.centerIn: parent
                            width: Theme.iconSizeMedium
                            height: Theme.iconSizeMedium
                            source: "image://theme/icon-m-left?#FFFFFF"
                            opacity: parent.highlighted ? 0.5 : 1
                        }
                    }

                    Label {
                        width: parent.width - Theme.itemSizeSmall * 2
                        height: parent.height
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        truncationMode: TruncationMode.Fade
                        color: "white"
                        font.pixelSize: Theme.fontSizeMedium
                        font.family: Theme.fontFamilyHeading
                        text: root.weekTitle || root.textFor("week")
                    }

                    BackgroundItem {
                        width: Theme.itemSizeSmall
                        height: parent.height
                        onClicked: root.weekOffset += 1
                        Image {
                            anchors.centerIn: parent
                            width: Theme.iconSizeMedium
                            height: Theme.iconSizeMedium
                            source: "image://theme/icon-m-right?#FFFFFF"
                            opacity: parent.highlighted ? 0.5 : 1
                        }
                    }
                }

                Label {
                    width: parent.width
                    visible: root.statusLine.length > 0
                    height: visible ? implicitHeight : 0
                    wrapMode: Text.Wrap
                    color: "#CCFFFFFF"
                    font.pixelSize: Theme.fontSizeSmall
                    text: root.statusLine
                }

                Column {
                    id: weekCol
                    width: parent.width
                    spacing: Theme.paddingSmall / 2
                    visible: root.weekRows.length > 0

                    readonly property int timeWidth: Math.max(Theme.itemSizeSmall,
                                                              Math.round(width * 0.14))
                    readonly property int dayWidth: Math.max(1, Math.floor((width - timeWidth) / 5))
                    readonly property int cellHeight: Theme.itemSizeSmall * 0.72

                    Row {
                        width: weekCol.width
                        height: Theme.fontSizeSmall * 2.6

                        Item { width: weekCol.timeWidth; height: 1 }

                        Repeater {
                            model: root.weekHeaders
                            delegate: Column {
                                width: weekCol.dayWidth
                                height: parent.height

                                Label {
                                    width: parent.width
                                    horizontalAlignment: Text.AlignHCenter
                                    font.pixelSize: Theme.fontSizeExtraSmall
                                    font.bold: modelData.today
                                    color: modelData.today ? Theme.highlightColor : "white"
                                    text: modelData.label
                                }
                                Label {
                                    width: parent.width
                                    horizontalAlignment: Text.AlignHCenter
                                    font.pixelSize: Theme.fontSizeExtraSmall
                                    color: modelData.today ? "#CCFFFFFF" : "#99FFFFFF"
                                    text: modelData.date
                                }
                            }
                        }
                    }

                    Repeater {
                        model: root.weekRows
                        delegate: Row {
                            width: weekCol.width
                            height: weekCol.cellHeight

                            Label {
                                width: weekCol.timeWidth
                                height: parent.height
                                verticalAlignment: Text.AlignVCenter
                                font.pixelSize: Theme.fontSizeExtraSmall
                                color: "#CCFFFFFF"
                                text: modelData.start || ""
                            }

                            Repeater {
                                model: modelData.cells
                                delegate: Item {
                                    width: weekCol.dayWidth
                                    height: weekCol.cellHeight

                                    Rectangle {
                                        anchors.centerIn: parent
                                        width: parent.width - Theme.paddingSmall
                                        height: parent.height - Theme.paddingSmall / 2
                                        radius: Theme.paddingSmall
                                        color: modelData.label.length ? modelData.color : "transparent"
                                        opacity: modelData.label.length ? 0.95 : 0

                                        Label {
                                            anchors.centerIn: parent
                                            width: parent.width - 2
                                            horizontalAlignment: Text.AlignHCenter
                                            elide: Text.ElideRight
                                            font.pixelSize: modelData.meal
                                                            ? Theme.fontSizeSmall
                                                            : Theme.fontSizeExtraSmall
                                            font.bold: !modelData.meal
                                            color: "white"
                                            text: modelData.label
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
