import QtQuick
import QtQuick.Layouts
import FluentUI

FluScrollablePage {
    title: "传输管理"
    launchMode: FluPageType.SingleTask

    FluPivot {
        anchors.fill: parent
        FluPivotItem {
            title: "上传"
            FluTableView {
                anchors { fill: parent; margins: 8 }
                columnSource: backend.transferColumns
                dataSource: backend.uploadDataSource
            }
        }
        FluPivotItem {
            title: "下载"
            FluTableView {
                anchors { fill: parent; margins: 8 }
                columnSource: backend.transferColumns
                dataSource: backend.downloadDataSource
            }
        }
    }
}
