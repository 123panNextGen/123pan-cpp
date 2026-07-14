import QtQuick
import QtQuick.Layouts
import FluentUI

FluScrollablePage {
    title: "设置"
    launchMode: FluPageType.SingleTask

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 16

        FluGroupBox { title: "下载设置"; Layout.fillWidth: true
            ColumnLayout { spacing: 8
                FluButton { text: "下载目录: " + (backend.downloadPath||"未设置"); Layout.fillWidth: true; onClicked: backend.pickDownloadPath() }
                FluToggleSwitch { text: "每次询问"; checked: backend.askDownloadLocation; onCheckedChanged: backend.askDownloadLocation=checked }
                FluToggleSwitch { text: "多线程下载"; checked: backend.multiThreadDownload; onCheckedChanged: backend.multiThreadDownload=checked }
                RowLayout { FluText { text: "下载限速 KB/s"; Layout.fillWidth: true }
                    FluSpinBox { value: backend.downloadSpeedLimit; from:0; to:1048576; stepSize:100; onValueChanged: backend.downloadSpeedLimit=value } }
                RowLayout { FluText { text: "上传限速 KB/s"; Layout.fillWidth: true }
                    FluSpinBox { value: backend.uploadSpeedLimit; from:0; to:1048576; stepSize:100; onValueChanged: backend.uploadSpeedLimit=value } }
            }
        }
        FluGroupBox { title: "网络代理"; Layout.fillWidth: true
            ColumnLayout { spacing: 8
                FluToggleSwitch { text: "启用代理"; checked: backend.proxyEnabled; onCheckedChanged: backend.proxyEnabled=checked }
                FluComboBox { model:["HTTP","SOCKS5"]; currentIndex: backend.proxyType==="socks5"?1:0; onCurrentValueChanged: backend.proxyType=(currentIndex===1?"socks5":"http"); Layout.fillWidth: true }
                FluTextBox { placeholderText: "代理主机"; text: backend.proxyHost; onTextChanged: backend.proxyHost=text; Layout.fillWidth: true }
                RowLayout { FluText { text: "端口"; Layout.fillWidth: true }
                    FluSpinBox { value: backend.proxyPort; from:0; to:65535; onValueChanged: backend.proxyPort=value } }
            }
        }
        FluGroupBox { title: "调试"; Layout.fillWidth: true
            ColumnLayout { spacing: 8
                FluComboBox { model: backend.logLevels; currentIndex: backend.logLevelIndex; onCurrentValueChanged: backend.logLevelIndex=currentIndex; Layout.fillWidth: true }
                FluButton { text: "打开日志文件"; onClicked: backend.openLogFile() }
            }
        }
        FluGroupBox { title: "关于"; Layout.fillWidth: true
            ColumnLayout { spacing: 8
                FluText { text: "123pan v" + backend.appVersion; font: FluTextStyle.Body }
                FluButton { text: "项目页面"; onClicked: backend.openProjectPage() }
                FluButton { text: "检查更新"; onClicked: backend.checkUpdate() }
            }
        }
    }
}
