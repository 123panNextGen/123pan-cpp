import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FluentUI

FluContentPage {
    id: filePage
    title: "文件管理"

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        // Toolbar
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            FluButton {
                text: "← 返回上级"
                enabled: backend.fileCurrentDirId !== 0
                onClicked: backend.fileGoParentDir()
            }

            FluBreadcrumbBar {
                id: breadcrumb
                Layout.fillWidth: true
                model: backend.fileBreadcrumb
                onCurrentIndexChanged: backend.fileNavigateTo(breadcrumb.currentIndex)
            }

            FluButton { text: "新建文件夹"; onClicked: backend.fileCreateFolder() }
            FluButton { text: "上传"; onClicked: backend.fileUpload() }
            FluButton { text: "下载"; onClicked: backend.fileDownload() }
            FluButton { text: "删除"; onClicked: backend.fileDelete() }
            FluButton { text: "刷新"; onClicked: backend.fileRefresh() }
        }

        // Main content: tree + table
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true

            // Folder tree (left sidebar)
            FluTreeView {
                id: folderTree
                Layout.preferredWidth: 220
                Layout.fillHeight: true
                model: backend.fileTreeModel
                onCurrentIndexChanged: {
                    if (currentIndex >= 0) {
                        backend.fileNavigateTo(currentIndex)
                    }
                }
            }

            // File list table (right)
            FluTableView {
                id: fileTable
                Layout.fillWidth: true
                Layout.fillHeight: true
                model: backend.fileTableModel
                onDoubleClicked: function(row) {
                    backend.fileOpenItem(row)
                }
            }
        }
    }

    Connections {
        target: backend
        function onFileListChanged() {
            // QML models auto-update
        }
        function onStorageInfoChanged(usedText) {
            // Update storage display if needed
        }
    }
}
