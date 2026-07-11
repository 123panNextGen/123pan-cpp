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
                FluTableView {
                    anchors { fill: parent; margins: 8 }
                    columnSource: backend.transferColumns
                    dataSource: backend.uploadDataSource
                }
            }
        }
        FluPivotItem {
            title: "下载"
            contentItem: Component {
                FluTableView {
                    anchors { fill: parent; margins: 8 }
                    columnSource: backend.transferColumns
                    dataSource: backend.downloadDataSource
                }
            }
        }
    }
}
