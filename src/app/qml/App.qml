import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FluentUI

FluWindow {
    id: mainWindow
    title: "123pan"
    width: 1000
    height: 680
    minimumWidth: 800
    minimumHeight: 500
    launchMode: FluWindowType.SingleTask

    appBar: FluAppBar {
        height: 30
        showDark: true
    }

    FluNavigationView {
        id: navView
        anchors.fill: parent
        topPadding: 0
        displayMode: FluNavigationViewType.Open
        pageMode: FluNavigationViewType.Stack
        cellWidth: 200
        items: FluObject {
            FluPaneItem {
                title: "文件"
                icon: FluentIcons.Document
                url: "qrc:/qml/FilePage.qml"
                onTapListener: function() { navView.push("qrc:/qml/FilePage.qml") }
            }
            FluPaneItem {
                title: "传输"
                icon: FluentIcons.Sync
                url: "qrc:/qml/TransferPage.qml"
                onTapListener: function() { navView.push("qrc:/qml/TransferPage.qml") }
            }
            FluPaneItem {
                title: "云盘"
                icon: FluentIcons.Cloud
                url: "qrc:/qml/CloudPage.qml"
                onTapListener: function() { navView.push("qrc:/qml/CloudPage.qml") }
            }
            FluPaneItem {
                title: "设置"
                icon: FluentIcons.Settings
                url: "qrc:/qml/SettingsPage.qml"
                onTapListener: function() { navView.push("qrc:/qml/SettingsPage.qml") }
            }
        }
        footerItems: FluObject {
            FluPaneItem {
                title: "退出登录"
                icon: FluentIcons.SignOut
                onTap: {
                    backend.logout()
                }
            }
        }
    }

    FluLoader {
        id: loginLoader
        anchors.fill: parent
        z: 10
    }

    Component.onCompleted: {
        // Apply theme: match old QSS style, supports dark mode
        FluTheme.primaryColor = "#0078d4"
        FluTheme.nativeText = true
        FluTheme.animationEnabled = true
        backend.init()
        if (!backend.isLoggedIn) {
            loginLoader.source = "qrc:/qml/LoginPage.qml"
        }
    }

    // Delay initial page load until NavigationView is fully initialized
    Timer {
        id: initTimer
        interval: 100
        running: true
        repeat: false
        onTriggered: {
            navView.push("qrc:/qml/FilePage.qml")
            navView.setCurrentIndex(0)
        }
    }

    Connections {
        target: backend
        function onLoginSuccess() {
            loginLoader.source = ""
        }
        function onLogoutRequested() {
            loginLoader.source = "qrc:/qml/LoginPage.qml"
        }
    }
}
