import QtQuick
import QtQuick.Layouts
import FluentUI

FluScrollablePage {
    title: "传输管理"
    launchMode: FluPageType.SingleTask

    FluPivot {
        Layout.fillWidth: true
        Layout.fillHeight: true
        FluPivotItem {
            title: "上传"
            contentItem: Component {
                ListView {
                    anchors.fill: parent; clip: true
                    model: backend.uploadDataSource
                    delegate: Rectangle {
                        width: parent.width; height: 32; color: index%2?"transparent":(FluTheme.dark?"#2a2a2a":"#f9f9f9")
                        RowLayout {
                            anchors { fill:parent; margins:4 }
                            FluText { text: modelData.name||""; Layout.fillWidth: true }
                            FluText { text: modelData.size||""; Layout.preferredWidth: 90 }
                            FluProgressBar { Layout.preferredWidth: 140; value: 0 }
                            FluText { text: modelData.status||""; Layout.preferredWidth: 80 }
                        }
                    }
                }
            }
        }
        FluPivotItem {
            title: "下载"
            contentItem: Component {
                ListView {
                    anchors.fill: parent; clip: true
                    model: backend.downloadDataSource
                    delegate: Rectangle {
                        width: parent.width; height: 32; color: index%2?"transparent":(FluTheme.dark?"#2a2a2a":"#f9f9f9")
                        RowLayout {
                            anchors { fill:parent; margins:4 }
                            FluText { text: modelData.name||""; Layout.fillWidth: true }
                            FluText { text: modelData.size||""; Layout.preferredWidth: 90 }
                            FluProgressBar { Layout.preferredWidth: 140; value: 0 }
                            FluText { text: modelData.status||""; Layout.preferredWidth: 80 }
                        }
                    }
                }
            }
        }
    }
}
