import QtQuick
import QtQuick.Layouts
import FluentUI

FluWindow {
    id: loginWindow
    title: "123pan - 登录"
    width: 400
    height: 340
    fixSize: true
    launchMode: FluWindowType.SingleTask

    appBar: FluAppBar {
        height: 30
        showDark: true
        showClose: true
        showMinimize: false
        showMaximize: false
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: 320
        spacing: 16

        FluText {
            text: "欢迎使用123云盘"
            font: FluTextStyle.Title
            Layout.alignment: Qt.AlignHCenter
        }

        FluTextBox {
            id: inpAcc
            placeholderText: "账户名"
            Layout.fillWidth: true
        }

        FluPasswordBox {
            id: inpPwd
            placeholderText: "密码"
            Layout.fillWidth: true
        }

        FluFilledButton {
            text: "登录"
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            enabled: inpAcc.text.length > 0 && inpPwd.text.length > 0
            onClicked: backend.login(inpAcc.text, inpPwd.text)
        }

        FluText {
            id: errText
            font: FluTextStyle.Caption
            color: "#e81123"
            Layout.alignment: Qt.AlignHCenter
            visible: text.length > 0
        }
    }

    Connections {
        target: backend
        function onLoginFailed(msg) {
            errText.text = msg
        }
    }

    onClosing: {
        if (!backend.isLoggedIn) {
            Qt.quit()
        }
    }
}
