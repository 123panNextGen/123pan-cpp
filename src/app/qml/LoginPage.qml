import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FluentUI

FluContentPage {
    id: loginPage
    title: "登录123云盘"

    ColumnLayout {
        anchors.centerIn: parent
        width: 400
        spacing: 20

        FluText {
            text: "欢迎使用123云盘"
            font: FluTextStyle.Title
            Layout.alignment: Qt.AlignHCenter
        }

        FluTextBox {
            id: accountInput
            placeholderText: "账户名"
            Layout.fillWidth: true
        }

        FluPasswordBox {
            id: passwordInput
            placeholderText: "密码"
            Layout.fillWidth: true
        }

        FluFilledButton {
            text: "登录"
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            enabled: accountInput.text.length > 0 && passwordInput.text.length > 0
            onClicked: {
                loginPage.enabled = false
                backend.login(accountInput.text, passwordInput.text)
            }
        }

        FluText {
            text: loginPage.statusText || ""
            font: FluTextStyle.Caption
            color: "#e81123"
            Layout.alignment: Qt.AlignHCenter
            visible: text.length > 0
        }
    }

    property string statusText: ""

    Connections {
        target: backend
        function onLoginFailed(message) {
            loginPage.statusText = message
            loginPage.enabled = true
        }
    }
}
