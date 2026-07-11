import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FluentUI

FluContentPage {
    id: cloudPage
    title: "云盘信息"

    ColumnLayout {
        anchors.fill: parent
        spacing: 20
        anchors.margins: 24

        FluText {
            text: "云盘信息"
            font: FluTextStyle.TitleLarge
        }

        FluGroupBox {
            title: "账户信息"
            Layout.fillWidth: true

            ColumnLayout {
                spacing: 12
                FluText {
                    text: "用户名: " + backend.currentUserName
                    font: FluTextStyle.Body
                }
                FluText {
                    text: "设备: " + backend.currentDeviceInfo
                    font: FluTextStyle.Caption
                }
            }
        }

        FluButton {
            text: "切换账号"
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            onClicked: backend.switchAccount()
        }

        FluFilledButton {
            text: "退出登录"
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            color: "#e81123"
            onClicked: backend.logout()
        }
    }
}
