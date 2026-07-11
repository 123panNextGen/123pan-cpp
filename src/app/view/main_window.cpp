#include "app/view/main_window.hpp"
#include "app/view/file_interface.hpp"
#include "app/view/transfer_interface.hpp"
#include "app/view/setting_interface.hpp"
#include "app/view/cloud_interface.hpp"
#include "app/view/login_window.hpp"
#include "app/common/api.hpp"
#include "app/common/config.hpp"
#include "app/common/credential.hpp"
#include "app/common/log.hpp"

#include <QHBoxLayout>
#include <QSplitter>
#include <QMessageBox>
#include <QApplication>
#include <QTimer>
#include <QCloseEvent>

namespace app {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("123pan");
    resize(900, 600);

    auto logger = get_logger("ui");
    logger->info("MainWindow 初始化");

    // Create central widget with horizontal layout
    auto* central = new QWidget(this);
    setCentralWidget(central);
    auto* main_layout = new QHBoxLayout(central);
    main_layout->setContentsMargins(0, 0, 0, 0);
    main_layout->setSpacing(0);

    // Navigation sidebar
    _nav_list = new QListWidget(central);
    _nav_list->setObjectName("navList");
    _nav_list->setFixedWidth(180);
    _nav_list->setSpacing(2);
    _nav_list->addItem("📁 文件");
    _nav_list->addItem("📥 传输");
    _nav_list->addItem("☁ 云盘");
    _nav_list->addItem("⚙ 设置");

    main_layout->addWidget(_nav_list);

    // Content area
    _stacked_widget = new QStackedWidget(central);

    _file_interface = new FileInterface(_stacked_widget);
    _transfer_interface = new TransferInterface(_stacked_widget);
    _cloud_interface = new CloudInterface(_stacked_widget);
    _setting_interface = new SettingInterface(_stacked_widget);

    _file_interface->set_transfer_interface(_transfer_interface);

    _stacked_widget->addWidget(_file_interface);       // index 0
    _stacked_widget->addWidget(_transfer_interface);    // index 1
    _stacked_widget->addWidget(_cloud_interface);       // index 2
    _stacked_widget->addWidget(_setting_interface);     // index 3

    main_layout->addWidget(_stacked_widget, 1);

    // Connect navigation
    connect(_nav_list, &QListWidget::currentRowChanged,
            _stacked_widget, &QStackedWidget::setCurrentIndex);

    // Connect cloud interface signals
    connect(_cloud_interface, &CloudInterface::logout_requested,
            this, &MainWindow::handle_logout);
    connect(_cloud_interface, &CloudInterface::switch_account_requested,
            this, &MainWindow::handle_switch_account);

    // Select first item
    _nav_list->setCurrentRow(0);

    // Start login flow
    QTimer::singleShot(0, this, &MainWindow::startup_login_flow);

    logger->info("MainWindow 初始化完成");
}

void MainWindow::startup_login_flow() {
    auto logger = get_logger("ui");
    bool cfg_loaded = false;

    std::string current_account = ConfigManager::get_current_account_name();
    json account_info = ConfigManager::get_account(current_account);

    logger->info("启动登录流程: current_account={}, has_password={}",
                current_account.empty() ? "(无)" : current_account,
                account_info.contains("passWord") && !account_info["passWord"].get<std::string>().empty());

    if (!account_info.empty() && account_info.contains("passWord")) {
        std::string pwd = account_info["passWord"].get<std::string>();
        if (!pwd.empty() && pwd.starts_with("enc:")) {
            pwd = decrypt_credential(pwd);
        }
        if (!pwd.empty()) {
            try {
                logger->debug("尝试自动登录");
                _pan = std::make_shared<Pan123>(true, current_account, pwd);
                auto [code, items] = _pan->get_dir(false);

                if (code == 0) {
                    cfg_loaded = true;
                    logger->info("自动登录成功: {}", current_account);
                } else {
                    cfg_loaded = false;
                    logger->warn("自动登录失败: get_dir 返回 code={}", code);
                }
            } catch (const std::exception& e) {
                cfg_loaded = false;
                logger->warn("自动登录异常: {}", e.what());
            }
        }
    }

    if (!cfg_loaded) {
        logger->debug("显示登录对话框");
        LoginDialog dlg(this);
        if (dlg.exec() != QDialog::Accepted) {
            logger->info("用户取消登录，退出程序");
            QTimer::singleShot(0, this, &QWidget::close);
            return;
        }
        _pan = dlg.get_pan();
        logger->info("登录成功: {}", _pan->user_name);
    }

    // Set up interfaces with Pan123
    _file_interface->set_pan(_pan);
    _file_interface->load_pan_and_data();
    _transfer_interface->set_pan(_pan);
    _cloud_interface->set_pan(_pan);
    _setting_interface->set_pan(_pan);
}

void MainWindow::clear_login_config() {
    ConfigManager::set_current_account("");
    auto logger = get_logger("ui");
    logger->info("登录配置已清除");
}

void MainWindow::handle_logout() {
    auto logger = get_logger("ui");
    logger->info("用户请求退出登录");

    auto result = QMessageBox::question(
        this, "退出登录", "确定要退出登录吗？",
        QMessageBox::Yes | QMessageBox::No
    );

    if (result != QMessageBox::Yes) {
        logger->debug("用户取消退出登录");
        return;
    }

    logger->debug("确认退出登录");
    clear_login_config();

    LoginDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        _pan = dlg.get_pan();
        logger->info("新登录成功: {}", _pan->user_name);
        _file_interface->set_pan(_pan);
        _file_interface->load_pan_and_data();
        _transfer_interface->set_pan(_pan);
        _cloud_interface->set_pan(_pan);
        _setting_interface->set_pan(_pan);
    } else {
        logger->info("用户取消重新登录，退出程序");
        close();
    }
}

void MainWindow::handle_switch_account() {
    auto logger = get_logger("ui");
    logger->info("用户请求切换账号");

    LoginDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        _pan = dlg.get_pan();
        logger->info("切换账号成功: {}", _pan->user_name);
        _file_interface->set_pan(_pan);
        _file_interface->load_pan_and_data();
        _transfer_interface->set_pan(_pan);
        _cloud_interface->set_pan(_pan);
        _setting_interface->set_pan(_pan);
    } else {
        logger->debug("用户取消切换账号");
    }
}

}  // namespace app
