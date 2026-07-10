#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>

namespace app {

/// 通用文本输入弹窗（取代 NewFolderDialog 和 RenameDialog）。
class InputDialog : public QDialog {
    Q_OBJECT
public:
    explicit InputDialog(const QString& title, const QString& hint,
                        const QString& default_text = "",
                        QWidget* parent = nullptr);

    /// 获取用户输入的文本。
    [[nodiscard]] QString get_input_text() const;

private:
    QLineEdit* _input;
};

}  // namespace app
