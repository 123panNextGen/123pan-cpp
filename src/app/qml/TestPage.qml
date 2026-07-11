import QtQuick
import FluentUI

Item {
    anchors.fill: parent
    Rectangle {
        anchors.fill: parent
        color: FluTheme.dark ? "#1e1e1e" : "#ffffff"
        Text {
            anchors.centerIn: parent
            text: "文件管理 — FluentUI"
            color: FluTheme.fontPrimaryColor
            font.pixelSize: 24
        }
    }
}
