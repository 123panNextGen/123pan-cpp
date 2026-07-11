import QtQuick
import QtQuick.Layouts
import FluentUI

FluScrollablePage {
    title: "文件管理"
    launchMode: FluPageType.SingleTask

    ColumnLayout {
        spacing: 8
        Layout.fillWidth: true

        // Toolbar
        RowLayout {
            spacing: 6
            FluButton {
                text: "← 返回上级"
                enabled: backend.fileCurrentDirId !== 0
                onClicked: backend.fileGoParentDir()
            }
            FluButton { text: "刷新"; onClicked: backend.fileRefresh() }
        }

        // Quick create folder row
        RowLayout {
            Layout.fillWidth: true
            FluTextBox {
                id: newFolderInput
                placeholderText: "新文件夹名称"
                Layout.fillWidth: true
            }
            FluButton {
                text: "创建"
                enabled: newFolderInput.text.length > 0
                onClicked: {
                    backend.fileCreateFolder(newFolderInput.text)
                    newFolderInput.text = ""
                }
            }
        }

        // File table
        FluTableView {
            id: fileTable
            Layout.fillWidth: true
            Layout.fillHeight: true
            columnSource: backend.fileTableColumns
            dataSource: backend.fileTableDataSource
        }
    }
}
