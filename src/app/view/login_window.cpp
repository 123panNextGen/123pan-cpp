#include "app/view/login_window.hpp"
#include "app/common/api.hpp"
#include "app/common/config.hpp"
#include "app/common/credential.hpp"
#include "app/common/log.hpp"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QApplication>

namespace app {

LoginDialog::LoginDialog(QWidget* parent)
    : QDialog(parent)
{
    auto logger = get_logger("ui");
    setWindowTitle("登录123云盘");
    resize(460, 320);
    setFixedSize(460, 320);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    logger->debug("LoginDialog 初始化");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(40, 30, 40, 30);
    layout->setSpacing(20);

    // Title
    auto* title = new QLabel("欢迎使用123云盘", this);
    title->setObjectName("loginTitle");
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    auto* form = new QFormLayout();
    form->setSpacing(15);

    // Account combobox
    _cbo_accounts = new QComboBox(this);
    _cbo_accounts->setEditable(true);
    _cbo_accounts->setInsertPolicy(QComboBox::NoInsert);
    _cbo_accounts->lineEdit()->setPlaceholderText("选择或输入账户");
    form->addRow("账户", _cbo_accounts);

    // Password input
    _le_pass = new QLineEdit(this);
    _le_pass->setPlaceholderText("请输入密码");
    _le_pass->setEchoMode(QLineEdit::Password);
    form->addRow("密码", _le_pass);

    layout->addLayout(form);

    auto* h = new QHBoxLayout();
    h->addStretch();

    _btn_ok = new QPushButton("登录", this);
    _btn_ok->setMinimumWidth(100);
    _btn_ok->setDefault(true);

    _btn_cancel = new QPushButton("取消", this);
    _btn_cancel->setMinimumWidth(100);

    h->addWidget(_btn_ok);
    h->addWidget(_btn_cancel);
    layout->addLayout(h);

    connect(_btn_ok, &QPushButton::clicked, this, &LoginDialog::on_ok);
    connect(_btn_cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(_cbo_accounts, &QComboBox::currentTextChanged,
            this, &LoginDialog::on_account_selected);

    // Load saved accounts
    auto account_names = ConfigManager::get_account_names();
    for (auto& name : account_names) {
        _cbo_accounts->addItem(QString::fromStdString(name));
    }

    std::string current = ConfigManager::get_current_account_name();
    if (!current.empty()) {
        QString cur = QString::fromStdString(current);
        if (_cbo_accounts->findText(cur) >= 0) {
            _cbo_accounts->setCurrentText(cur);
        } else {
            _cbo_accounts->addItem(cur);
            _cbo_accounts->setCurrentText(cur);
        }
    }

    on_account_selected(_cbo_accounts->currentText());
}

void LoginDialog::on_account_selected(const QString& account_name) {
    auto logger = get_logger("ui");
    auto trimmed = account_name.trimmed();
    if (trimmed.isEmpty()) return;

    json account = ConfigManager::get_account(trimmed.toStdString());
    if (!account.empty()) {
        logger->debug("已加载账号 {} 的密码", trimmed.toStdString());
        std::string pwd = account.value("passWord", "");
        if (!pwd.empty() && pwd.starts_with("enc:")) {
            pwd = decrypt_credential(pwd);
        }
        _le_pass->setText(QString::fromStdString(pwd));
    } else {
        logger->debug("账号 {} 无保存信息", trimmed.toStdString());
    }
}

void LoginDialog::on_ok() {
    auto logger = get_logger("ui");
    QString user = _cbo_accounts->currentText().trimmed();
    QString pwd = _le_pass->text();

    logger->info("登录尝试: user={}, has_pwd={}",
                user.toStdString(), !pwd.isEmpty());

    if (user.isEmpty() || pwd.isEmpty()) {
        logger->warn("登录失败: 用户名或密码为空");
        QMessageBox::warning(this, "提示", "请输入用户名和密码。");
        return;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);

    try {
        json account = ConfigManager::get_account(user.toStdString());
        if (!account.empty()) {
            logger->debug("使用已保存账号信息登录: {}", user.toStdString());
            _pan = std::make_shared<Pan123>(true, user.toStdString(),
                                            pwd.toStdString());
        } else {
            logger->debug("新账号登录: {}", user.toStdString());
            _pan = std::make_shared<Pan123>(false, user.toStdString(),
                                            pwd.toStdString());
        }

        int code = _pan->login();
        logger->info("登录结果: user={}, code={}", user.toStdString(), code);

        if (code != 200 && code != 0) {
            _login_error = QString("登录失败，返回码: %1").arg(code);
            QApplication::restoreOverrideCursor();
            QMessageBox::warning(this, "登录失败", _login_error);
            _pan.reset();
            return;
        }
    } catch (const std::exception& e) {
        _login_error = QString::fromStdString(e.what());
        logger->error("登录异常: {}", e.what());
        QApplication::restoreOverrideCursor();
        QMessageBox::critical(this, "登录异常",
                             QString("登录时发生异常:\n") + e.what());
        _pan.reset();
        return;
    }

    QApplication::restoreOverrideCursor();

    // Save account to config
    try {
        _pan->save_file();
    } catch (const std::exception& e) {
        logger->warn("保存配置失败: {}", e.what());
    }

    logger->info("登录成功，对话框关闭: {}", user.toStdString());
    accept();
}

}  // namespace app
