#pragma once

#include <QDialog>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <memory>

namespace app {

class Pan123;

/// 登录对话框。
class LoginDialog : public QDialog {
    Q_OBJECT
public:
    explicit LoginDialog(QWidget* parent = nullptr);

    /// 获取登录成功的 Pan123 对象。
    [[nodiscard]] std::shared_ptr<Pan123> get_pan() const { return _pan; }

private slots:
    void on_account_selected(const QString& account_name);
    void on_ok();

private:
    QComboBox* _cbo_accounts;
    QLineEdit* _le_pass;
    QPushButton* _btn_ok;
    QPushButton* _btn_cancel;

    std::shared_ptr<Pan123> _pan;
    QString _login_error;
};

}  // namespace app
