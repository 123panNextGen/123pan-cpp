import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FluentUI

FluContentPage {
    id: transferPage
    title: "传输管理"

    FluPivot {
        anchors.fill: parent

        FluPivotItem {
            title: "上传"

            FluTableView {
                anchors.fill: parent
                model: backend.uploadTaskModel
                columns: [
                    { title: "文件名", dataIndex: "name", width: 200 },
                    { title: "大小", dataIndex: "size", width: 100 },
                    { title: "进度", dataIndex: "progress", width: 150 },
                    { title: "状态", dataIndex: "status", width: 100 }
                ]
            }
        }

        FluPivotItem {
            title: "下载"

            FluTableView {
                anchors.fill: parent
                model: backend.downloadTaskModel
                columns: [
                    { title: "文件名", dataIndex: "name", width: 200 },
                    { title: "大小", dataIndex: "size", width: 100 },
                    { title: "进度", dataIndex: "progress", width: 150 },
                    { title: "状态", dataIndex: "status", width: 100 }
                ]
            }
        }
    }
}
