import QtQuick 2.6
import Sailfish.Silica 1.0

CoverBackground {
    id: cover
    property string schoolName: ""
    property string schoolHost: ""
    property bool configured: false
    property bool loggedIn: false
    property int unreadCount: 0
    property string notificationTitle: ""
    property string notificationBody: ""
    property bool showingNotification: false
    signal requestSettings
    signal requestReload

    transparent: true

    Rectangle {
        anchors.fill: parent
        color: "#0B365C"
        opacity: 0.36
    }

    Item {
        id: capWatermark
        anchors.centerIn: parent
        width: parent.width * 1.35
        height: width
        opacity: 0.22
        rotation: -18
        z: 0

        Canvas {
            id: capCanvas
            anchors.fill: parent
            antialiasing: true

            onPaint: {
                var ctx = getContext("2d")
                var w = width
                var h = height
                ctx.clearRect(0, 0, w, h)
                ctx.fillStyle = "#FFFFFF"
                ctx.strokeStyle = "#FFFFFF"
                ctx.lineCap = "round"
                ctx.lineJoin = "round"

                var cx = w * 0.5
                var cy = h * 0.46
                var s = Math.min(w, h) * 0.30

                // Skull / band
                ctx.beginPath()
                ctx.ellipse(cx - s * 0.42, cy + s * 0.18, s * 0.84, s * 0.54)
                ctx.fill()

                // Mortarboard diamond
                ctx.beginPath()
                ctx.moveTo(cx, cy - s * 0.50)
                ctx.lineTo(cx + s * 1.08, cy + s * 0.08)
                ctx.lineTo(cx, cy + s * 0.58)
                ctx.lineTo(cx - s * 1.08, cy + s * 0.08)
                ctx.closePath()
                ctx.fill()

                // Center button
                var br = Math.max(4, s * 0.11)
                ctx.beginPath()
                ctx.arc(cx, cy - br * 0.15, br, 0, Math.PI * 2)
                ctx.fill()

                // Tassel cord
                ctx.lineWidth = Math.max(4, s * 0.11)
                ctx.beginPath()
                ctx.moveTo(cx + br * 0.4, cy + br * 0.6)
                ctx.quadraticCurveTo(
                            cx + s * 0.55, cy + s * 0.55,
                            cx + s * 0.78, cy + s * 1.05)
                ctx.stroke()

                // Tassel knot
                var tbx = cx + s * 0.78
                var tby = cy + s * 1.05
                var tw = s * 0.26
                var th = s * 0.24
                ctx.beginPath()
                ctx.ellipse(tbx - tw * 0.5, tby - th * 0.35, tw, th)
                ctx.fill()

                // Fringe
                ctx.lineWidth = Math.max(3, s * 0.08)
                var fringe = [
                            [-0.11, 0.38], [-0.04, 0.44], [0.04, 0.44], [0.11, 0.38]
                        ]
                for (var i = 0; i < fringe.length; i++) {
                    ctx.beginPath()
                    ctx.moveTo(tbx + fringe[i][0] * s * 0.5, tby + s * 0.10)
                    ctx.lineTo(tbx + fringe[i][0] * s, tby + fringe[i][1] * s)
                    ctx.stroke()
                }
            }

            Component.onCompleted: requestPaint()
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
        }
    }

    Column {
        anchors.centerIn: parent
        width: parent.width - Theme.paddingLarge * 2
        spacing: Theme.paddingSmall
        z: 1

        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            truncationMode: TruncationMode.Fade
            color: "white"
            font.pixelSize: Theme.fontSizeLarge
            font.bold: true
            text: cover.configured && cover.schoolName.length > 0
                  ? cover.schoolName
                  : qsTr("Admirality")
        }

        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            maximumLineCount: 3
            elide: Text.ElideRight
            color: "#F2FFFFFF"
            font.pixelSize: Theme.fontSizeTiny
            visible: cover.showingNotification && cover.notificationBody.length > 0
            text: cover.notificationBody
        }

        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            color: "#CCFFFFFF"
            font.pixelSize: Theme.fontSizeExtraSmall
            text: {
                if (cover.showingNotification && cover.notificationTitle.length > 0)
                    return cover.notificationTitle
                if (!cover.configured)
                    return qsTr("Not configured")
                if (!cover.loggedIn)
                    return qsTr("Not signed in")
                if (cover.unreadCount === 1)
                    return qsTr("1 unread message")
                if (cover.unreadCount > 1)
                    return qsTr("%1 unread messages").arg(cover.unreadCount)
                return qsTr("Wilma")
            }
        }

        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            truncationMode: TruncationMode.Fade
            color: "#B3FFFFFF"
            font.pixelSize: Theme.fontSizeTiny
            visible: cover.configured && cover.schoolHost.length > 0
            text: cover.schoolHost
        }
    }

    CoverActionList {
        CoverAction {
            iconSource: "image://theme/icon-cover-refresh"
            onTriggered: cover.requestReload()
        }
        CoverAction {
            iconSource: Qt.resolvedUrl("icon-cover-settings.png")
            onTriggered: cover.requestSettings()
        }
    }
}
