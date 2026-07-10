#pragma once

#include <QScrollArea>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QLineEdit>
#include <memory>

namespace app {

class Pan123;

/// 数值输入设置卡片。
class SpinBoxCard : public QWidget {
    Q_OBJECT
public:
    SpinBoxCard(const QString& title, const QString& desc,
               int value = 0, int min_val = 0, int max_val = 1048576,
               int step = 1, const QString& suffix = "",
               QWidget* parent = nullptr);

    void setValue(int val);
    int value() const;

signals:
    void value_changed(int val);

protected:
    QSpinBox* _spin_box;
};

/// 设置页面。
class SettingInterface : public QScrollArea {
    Q_OBJECT
public:
    explicit SettingInterface(QWidget* parent = nullptr);

    /// 设置 Pan123 引用（用于实时应用设置）。
    void set_pan(std::shared_ptr<Pan123> pan);

private slots:
    void on_download_folder_clicked();
    void on_ask_download_location_changed(bool checked);
    void on_multi_thread_changed(bool checked);
    void on_download_speed_changed(int val);
    void on_upload_speed_changed(int val);
    void on_proxy_enabled_changed(bool checked);
    void on_proxy_type_changed(const QString& text);
    void on_proxy_host_changed();
    void on_proxy_port_changed(int val);
    void on_proxy_user_changed();
    void on_proxy_pass_changed();
    void on_log_level_changed(const QString& level_name);
    void on_check_version();

private:
    void apply_proxy_to_service();
    void init_layout();

    QWidget* _scroll_widget;
    QVBoxLayout* _expand_layout;
    QLabel* _setting_label;

    // Download settings
    QPushButton* _download_folder_card;
    QCheckBox* _ask_download_check;
    QCheckBox* _multi_thread_check;
    SpinBoxCard* _download_speed_card;
    SpinBoxCard* _upload_speed_card;

    // Proxy settings
    QCheckBox* _proxy_enabled_check;
    QComboBox* _proxy_type_combo;
    QLineEdit* _proxy_host_edit;
    QSpinBox* _proxy_port_spin;
    QLineEdit* _proxy_user_edit;
    QLineEdit* _proxy_pass_edit;

    // Debug
    QComboBox* _log_level_combo;

    std::shared_ptr<Pan123> _pan;
};

}  // namespace app
