import QtQuick
import QtQuick.Layouts
import FluentUI

FluScrollablePage {
    title: "云盘信息"
    launchMode: FluPageType.SingleTask

    ColumnLayout {
        spacing: 16
        anchors { left: parent.left; right: parent.right; margins: 8 }

        FluGroupBox {
            title: "账户信息"
            Layout.fillWidth: true
            ColumnLayout {
                spacing: 8
                FluText {
                    text: "用户名: " + (backend.isLoggedIn ? backend.currentUserName : "未登录")
                    font: FluTextStyle.Body
                }
                FluText {
                    text: "设备: " + (backend.isLoggedIn ? backend.currentDeviceInfo : "-")
                    font: FluTextStyle.Caption
                }
            }
        }

        FluButton {
            text: "切换账号"
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            onClicked: backend.switchAccount()
        }

        FluFilledButton {
            text: "退出登录"
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            onClicked: backend.logout()
        }
    }
}
