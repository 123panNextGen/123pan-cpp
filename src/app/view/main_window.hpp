#pragma once

#include <QMainWindow>
#include <QStackedWidget>
#include <QListWidget>
#include <memory>

namespace app {

class Pan123;
class FileInterface;
class TransferInterface;
class SettingInterface;
class CloudInterface;
class LoginDialog;

/// 主窗口。
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

private slots:
    void handle_logout();
    void handle_switch_account();

private:
    void init_navigation();
    void startup_login_flow();
    void clear_login_config();

    // Navigation widgets
    QListWidget* _nav_list;
    QStackedWidget* _stacked_widget;

    // Interfaces
    std::shared_ptr<Pan123> _pan;
    FileInterface* _file_interface;
    TransferInterface* _transfer_interface;
    SettingInterface* _setting_interface;
    CloudInterface* _cloud_interface;
};

}  // namespace app
