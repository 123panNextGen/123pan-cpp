#include "app/view/cloud_interface.hpp"
#include "app/common/api.hpp"
#include "app/common/log.hpp"

#include <QGroupBox>
#include <QFormLayout>
#include <QFrame>
#include <QFont>

namespace app {

static QString mask_username(const QString& username) {
    if (username.isEmpty()) return {};

    // Check if it looks like a phone number (11 digits)
    if (username.length() == 11) {
        bool all_digits = true;
        for (auto& c : username) {
            if (!c.isDigit()) { all_digits = false; break; }
        }
        if (all_digits) {
            return username.left(3) + "****" + username.right(4);
        }
    }

    return username;
}

CloudInterface::CloudInterface(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("CloudInterface");

    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(24, 20, 24, 24);
    main_layout->setSpacing(12);

    // Title
    auto* title_label = new QLabel("云盘信息", this);
    QFont title_font;
    title_font.setPointSize(20);
    title_font.setBold(true);
    title_label->setFont(title_font);
    main_layout->addWidget(title_label);

    // Account info group
    auto* account_group = new QGroupBox("账户信息", this);
    auto* group_layout = new QFormLayout(account_group);
    group_layout->setSpacing(12);

    // Username display
    _username_label = new QLabel(this);
    QFont user_font;
    user_font.setPointSize(12);
    _username_label->setFont(user_font);
    group_layout->addRow("账户", _username_label);

    main_layout->addWidget(account_group);

    // Action buttons
    auto* switch_btn = new QPushButton("切换账号", this);
    switch_btn->setMinimumHeight(36);
    connect(switch_btn, &QPushButton::clicked, this, &CloudInterface::switch_account_requested);
    main_layout->addWidget(switch_btn);

    auto* logout_btn = new QPushButton("退出登录", this);
    logout_btn->setMinimumHeight(36);
    logout_btn->setStyleSheet("QPushButton { color: red; }");
    connect(logout_btn, &QPushButton::clicked, this, &CloudInterface::logout_requested);
    main_layout->addWidget(logout_btn);

    main_layout->addStretch();
}

void CloudInterface::set_pan(std::shared_ptr<Pan123> pan) {
    _pan = std::move(pan);
    if (_pan) {
        QString username = QString::fromStdString(_pan->user_name);
        _username_label->setText("用户名: " + mask_username(username));
        auto logger = get_logger("ui");
        logger->info("云盘界面已设置: {}", mask_username(username).toStdString());
    }
}

}  // namespace app
