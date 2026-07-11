import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import FluentUI

Item {
    id: page
    property var selId: 0
    property var selName: ""
    property var selType: 0
    property var fileListModel: []

    function refreshModel() { fileListModel = JSON.parse(backend.fileTableJson()) }
    Component.onCompleted: refreshModel()
    Connections { target: backend; function onFileListChanged() { refreshModel() } }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 4

        // Toolbar
        RowLayout {
            spacing: 4
            FluButton { text: "← 返回"; enabled: backend.fileCurrentDirId!==0; onClicked: backend.fileGoParentDir() }
            FluButton { text: "新建文件夹"; onClicked: dlgNewFolder.open() }
            FluButton { text: "上传"; onClicked: backend.fileUploadItem() }
            FluButton { text: "下载"; enabled: selId>0; onClicked: backend.fileDownloadItem(selId) }
            FluButton { text: "删除"; enabled: selId>0; onClicked: dlgDelete.open() }
            FluButton { text: "分享"; enabled: selId>0; onClicked: backend.fileShareItem(selId) }
            FluButton { text: "复制链接"; enabled: selId>0; onClicked: backend.fileCopyLink(selId) }
            FluButton { text: "🔄"; onClicked: backend.fileRefresh() }
        }

        // Storage bar
        RowLayout {
            FluText { text: "☁ 已用 " + backend.storageUsed + " / 共 " + backend.storageTotal + "  文件: " + fileListModel.length; font: FluTextStyle.Caption }
            FluProgressBar { Layout.fillWidth: true; Layout.preferredHeight: 6; value: backend.storagePercent }
        }

        // File list header
        Rectangle {
            Layout.fillWidth: true; height: 30; color: FluTheme.dark?"#3a3a3a":"#e8e8e8"; radius: 4
            RowLayout {
                anchors { fill:parent; margins:6 }
                FluText { text: "名称"; font: FluTextStyle.BodyStrong; Layout.fillWidth: true }
                FluText { text: "类型"; font: FluTextStyle.BodyStrong; Layout.preferredWidth: 100 }
                FluText { text: "大小"; font: FluTextStyle.BodyStrong; Layout.preferredWidth: 90 }
            }
        }

        // File list — fills ALL remaining space
        ListView {
            id: fileList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: fileListModel
            ScrollBar.vertical: FluScrollBar {}

            delegate: Rectangle {
                width: fileList.width; height: 34
                color: index%2===0 ? (FluTheme.dark?"#282828":"#f5f5f5") : "transparent"
                RowLayout {
                    anchors { fill:parent; margins:6 }
                    FluText { text: modelData.name||""; Layout.fillWidth: true; elide: Text.ElideRight }
                    FluText { text: modelData.typeName||""; Layout.preferredWidth: 100; font: FluTextStyle.Caption }
                    FluText { text: modelData.size||""; Layout.preferredWidth: 90; font: FluTextStyle.Caption }
                }
                MouseArea {
                    anchors.fill: parent; acceptedButtons: Qt.LeftButton|Qt.RightButton
                    onClicked: function(mouse) {
                        selId=modelData.fileId; selName=modelData.name; selType=modelData.type
                        if (mouse.button===Qt.RightButton) ctxMenu.popup()
                    }
                    onDoubleClicked: {
                        if (modelData.type===1) backend.fileNavigateToDir(modelData.fileId)
                        else backend.fileDownloadItem(modelData.fileId)
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
