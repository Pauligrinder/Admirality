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
        id: anchorWatermark
        anchors.centerIn: parent
        width: parent.width * 1.55
        height: width
        opacity: 0.22
        rotation: -28
        z: 0

        Canvas {
            id: anchorCanvas
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
                var cy = h * 0.52
                var s = Math.min(w, h) * 0.28

                ctx.lineWidth = Math.max(6, s * 0.18)
                ctx.beginPath()
                ctx.arc(cx, cy - s * 1.15, s * 0.22, 0, Math.PI * 2)
                ctx.stroke()

                ctx.beginPath()
                ctx.moveTo(cx - s * 0.72, cy - s * 0.72)
                ctx.lineTo(cx + s * 0.72, cy - s * 0.72)
                ctx.stroke()

                ctx.beginPath()
                ctx.moveTo(cx, cy - s * 0.93)
                ctx.lineTo(cx, cy + s * 0.78)
                ctx.stroke()

                ctx.beginPath()
                ctx.arc(cx, cy + s * 0.22, s * 0.95, Math.PI * 0.12, Math.PI * 0.88)
                ctx.stroke()

                ctx.beginPath()
                ctx.moveTo(cx - s * 0.95, cy + s * 0.38)
                ctx.lineTo(cx - s * 0.72, cy + s * 0.08)
                ctx.lineTo(cx - s * 0.55, cy + s * 0.42)
                ctx.closePath()
                ctx.fill()

                ctx.beginPath()
                ctx.moveTo(cx + s * 0.95, cy + s * 0.38)
                ctx.lineTo(cx + s * 0.72, cy + s * 0.08)
                ctx.lineTo(cx + s * 0.55, cy + s * 0.42)
                ctx.closePath()
                ctx.fill()
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
