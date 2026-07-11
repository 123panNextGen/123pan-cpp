import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import FluentUI

FluScrollablePage {
    id: page
    title: "文件管理"
    launchMode: FluPageType.SingleTask
    property var selId: 0
    property var selName: ""
    property var selType: 0

    ColumnLayout {
        spacing: 6
        Layout.fillWidth: true

        // Toolbar
        RowLayout {
            spacing: 6
            FluButton { text: "← 返回"; enabled: backend.fileCurrentDirId !== 0; onClicked: backend.fileGoParentDir() }
            FluButton { text: "新建文件夹"; onClicked: dlgNewFolder.open() }
            FluButton { text: "上传"; onClicked: backend.fileUploadItem() }
            FluButton { text: "下载"; enabled: selId>0; onClicked: backend.fileDownloadItem(selId) }
            FluButton { text: "删除"; enabled: selId>0; onClicked: dlgDelete.open() }
            FluButton { text: "分享"; enabled: selId>0; onClicked: backend.fileShareItem(selId) }
            FluButton { text: "复制链接"; enabled: selId>0; onClicked: backend.fileCopyLink(selId) }
            FluButton { text: "🔃 刷新"; onClicked: backend.fileRefresh() }
        }

        // Breadcrumb
        RowLayout {
            FluTextButton { text: "📁 根目录"; onClicked: backend.fileRefresh() }
            Repeater {
                model: backend.fileBreadcrumb
                delegate: Row {
                    FluText { text: " > "; font: FluTextStyle.Caption }
                    FluTextButton { text: "📁"; onClicked: backend.fileNavigateByBreadcrumb(index+1) }
                }
            }
        }

        // Content
        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 8

            ColumnLayout {
                Layout.preferredWidth: 220; Layout.fillHeight: true; spacing: 4
                FluTreeView {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    columnSource: [{title:"文件夹",dataIndex:"name",width:200}]
                    dataSource: backend.fileTreeDataSource
                }
                FluGroupBox {
                    title: "云盘空间"; Layout.fillWidth: true
                    ColumnLayout {
                        FluText { text: backend.storageUsed + " / " + backend.storageTotal; font: FluTextStyle.Caption }
                        FluProgressBar { Layout.fillWidth: true; value: backend.storagePercent }
                    }
                }
            }

            FluTableView {
                id: fileTable
                Layout.fillWidth: true; Layout.fillHeight: true
                columnSource: backend.fileTableColumns
                dataSource: backend.fileTableDataSource
                MouseArea {
                    anchors.fill: parent; acceptedButtons: Qt.RightButton
                    onClicked: function(mouse) {
                        var row = fileTable.view.rowAt(mouse.y)
                        if (row>=0) {
                            var d = backend.fileTableDataSource[row]; selId = d.fileId; selName = d.name; selType = d.type
                            ctxMenu.popup()
                        }
                    }
                }
                TapHandler {
                    onDoubleTapped: function(pt) {
                        var row = fileTable.view.rowAt(pt.position.y)
                        if (row>=0) {
                            var d = backend.fileTableDataSource[row]
                            if (d.type===1) backend.fileNavigateToDir(d.fileId)
                            else backend.fileDownloadItem(d.fileId)
                        }
                    }
                }
            }
        }
    }

    // Dialogs
    FluContentDialog { id: dlgNewFolder; title: "新建文件夹"; message: "输入文件夹名称："
        buttonFlags: FluContentDialogType.NegativeButton|FluContentDialogType.PositiveButton
        positiveText: "创建"; negativeText: "取消"
        contentDelegate: Component { FluTextBox { id: inpFolder; placeholderText: "文件夹名称"; width: 260 } }
        onPositiveClicked: { backend.fileCreateFolder(inpFolder.text); inpFolder.text="" }
    }
    FluContentDialog { id: dlgDelete; title: "确认删除"
        message: "确定要删除 \"" + selName + "\" 吗？"
        buttonFlags: FluContentDialogType.NegativeButton|FluContentDialogType.PositiveButton
        positiveText: "删除"; negativeText: "取消"
        onPositiveClicked: backend.fileDeleteItem(selId)
    }
    FluContentDialog { id: dlgRename; title: "重命名"; message: "输入新名称："
        buttonFlags: FluContentDialogType.NegativeButton|FluContentDialogType.PositiveButton
        positiveText: "确定"; negativeText: "取消"
        contentDelegate: Component { FluTextBox { id: inpRename; width: 260; text: selName } }
        onPositiveClicked: { backend.fileRenameItem(selId,inpRename.text) }
    }
    FluMenu { id: ctxMenu
        FluMenuItem { text: "下载"; onClicked: backend.fileDownloadItem(selId) }
        FluMenuItem { text: "删除"; onClicked: dlgDelete.open() }
        FluMenuItem { text: "重命名"; onClicked: dlgRename.open() }
        FluMenuItem { text: "分享"; onClicked: backend.fileShareItem(selId) }
        FluMenuItem { text: "复制链接"; onClicked: backend.fileCopyLink(selId) }
    }
}
