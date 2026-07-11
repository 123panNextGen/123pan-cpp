import QtQuick
import QtQuick.Layouts
import FluentUI

FluScrollablePage {
    title: "登录"
    launchMode: FluPageType.SingleTask

    ColumnLayout {
        anchors.centerIn: parent
        width: 360
        spacing: 16

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
            Layout.preferredHeight: 36
            enabled: accountInput.text.length > 0 && passwordInput.text.length > 0
            onClicked: backend.login(accountInput.text, passwordInput.text)
        }

        FluText {
            id: errorText
            font: FluTextStyle.Caption
            color: "#e81123"
            Layout.alignment: Qt.AlignHCenter
            visible: text.length > 0
        }
    }

    Connections {
        target: backend
        function onLoginFailed(msg) { errorText.text = msg }
    }
}
