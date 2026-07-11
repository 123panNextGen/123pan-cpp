#include "app/view/setting_interface.hpp"
#include "app/common/config.hpp"
#include "app/common/const.hpp"
#include "app/common/log.hpp"
#include "app/common/api.hpp"

#include <QFormLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QApplication>
#include <filesystem>

namespace app {

// ============================================================
// SpinBoxCard
// ============================================================
SpinBoxCard::SpinBoxCard(const QString& title, const QString& desc,
                       int value, int min_val, int max_val, int step,
                       const QString& suffix, QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 4, 8, 4);

    auto* label_layout = new QVBoxLayout();
    auto* title_label = new QLabel(title, this);
    title_label->setProperty("bold", true);
    label_layout->addWidget(title_label);

    auto* desc_label = new QLabel(desc, this);
    desc_label->setProperty("caption", true);
    label_layout->addWidget(desc_label);

    layout->addLayout(label_layout);
    layout->addStretch();

    _spin_box = new QSpinBox(this);
    _spin_box->setRange(min_val, max_val);
    _spin_box->setSingleStep(step);
    _spin_box->setValue(value);
    _spin_box->setSuffix(suffix);
    _spin_box->setMinimumWidth(160);
    layout->addWidget(_spin_box);

    connect(_spin_box, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &SpinBoxCard::value_changed);
}

void SpinBoxCard::setValue(int val) { _spin_box->setValue(val); }
int SpinBoxCard::value() const { return _spin_box->value(); }

// ============================================================
// SettingInterface
// ============================================================
SettingInterface::SettingInterface(QWidget* parent)
    : QScrollArea(parent)
{
    setObjectName("settingInterface");
    resize(1000, 800);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setWidgetResizable(true);

    _scroll_widget = new QWidget(this);
    _scroll_widget->setObjectName("scrollWidget");
    _expand_layout = new QVBoxLayout(_scroll_widget);
    _expand_layout->setSpacing(24);
    _expand_layout->setContentsMargins(36, 10, 36, 20);

    _setting_label = new QLabel("设置", _scroll_widget);
    _setting_label->setObjectName("settingLabel");
    _expand_layout->addWidget(_setting_label);

    // ---- 下载设置组 ----
    auto* download_group = new QGroupBox("下载设置", _scroll_widget);
    auto* dl_layout = new QVBoxLayout(download_group);
    dl_layout->setSpacing(8);

    _download_folder_card = new QPushButton("选择文件夹...", _scroll_widget);
    QString dl_path = QString::fromStdString(
        ConfigManager::get_setting<std::string>("defaultDownloadPath",
            (std::filesystem::path(getenv("HOME") ? getenv("HOME") : ".") / "Downloads").string())
    );
    _download_folder_card->setToolTip("当前下载目录: " + dl_path);
    dl_layout->addWidget(_download_folder_card);

    _ask_download_check = new QCheckBox("每次询问下载位置", _scroll_widget);
    _ask_download_check->setChecked(
        ConfigManager::get_setting<bool>("askDownloadLocation", true));
    dl_layout->addWidget(_ask_download_check);

    _multi_thread_check = new QCheckBox("多线程下载", _scroll_widget);
    _multi_thread_check->setToolTip("启用多线程分片下载以提升下载速度");
    _multi_thread_check->setChecked(
        ConfigManager::get_setting<bool>("multiThreadDownload", true));
    dl_layout->addWidget(_multi_thread_check);

    _download_speed_card = new SpinBoxCard("下载限速", "限制下载速度，0 表示不限制",
        ConfigManager::get_setting<int>("downloadSpeedLimit", 0),
        0, 1048576, 100, " KB/s", _scroll_widget);
    dl_layout->addWidget(_download_speed_card);

    _upload_speed_card = new SpinBoxCard("上传限速", "限制上传速度，0 表示不限制",
        ConfigManager::get_setting<int>("uploadSpeedLimit", 0),
        0, 1048576, 100, " KB/s", _scroll_widget);
    dl_layout->addWidget(_upload_speed_card);

    _expand_layout->addWidget(download_group);

    // ---- 代理设置组 ----
    auto* proxy_group = new QGroupBox("网络代理", _scroll_widget);
    auto* proxy_layout = new QVBoxLayout(proxy_group);
    proxy_layout->setSpacing(8);

    _proxy_enabled_check = new QCheckBox("启用代理", _scroll_widget);
    _proxy_enabled_check->setChecked(
        ConfigManager::get_setting<bool>("proxyEnabled", false));
    proxy_layout->addWidget(_proxy_enabled_check);

    auto* proxy_type_layout = new QHBoxLayout();
    proxy_type_layout->addWidget(new QLabel("代理类型:", _scroll_widget));
    _proxy_type_combo = new QComboBox(_scroll_widget);
    _proxy_type_combo->addItems({"HTTP", "SOCKS5"});
    std::string saved_type = ConfigManager::get_setting<std::string>("proxyType", "http");
    _proxy_type_combo->setCurrentIndex(saved_type == "socks5" ? 1 : 0);
    proxy_type_layout->addWidget(_proxy_type_combo);
    proxy_layout->addLayout(proxy_type_layout);

    auto* proxy_host_layout = new QHBoxLayout();
    proxy_host_layout->addWidget(new QLabel("代理主机:", _scroll_widget));
    _proxy_host_edit = new QLineEdit(_scroll_widget);
    _proxy_host_edit->setText(QString::fromStdString(
        ConfigManager::get_setting<std::string>("proxyHost", "")));
    proxy_host_layout->addWidget(_proxy_host_edit);
    proxy_layout->addLayout(proxy_host_layout);

    auto* proxy_port_layout = new QHBoxLayout();
    proxy_port_layout->addWidget(new QLabel("代理端口:", _scroll_widget));
    _proxy_port_spin = new QSpinBox(_scroll_widget);
    _proxy_port_spin->setRange(0, 65535);
    _proxy_port_spin->setValue(
        ConfigManager::get_setting<int>("proxyPort", 0));
    proxy_port_layout->addWidget(_proxy_port_spin);
    proxy_layout->addLayout(proxy_port_layout);

    auto* proxy_user_layout = new QHBoxLayout();
    proxy_user_layout->addWidget(new QLabel("代理用户名:", _scroll_widget));
    _proxy_user_edit = new QLineEdit(_scroll_widget);
    _proxy_user_edit->setText(QString::fromStdString(
        ConfigManager::get_setting<std::string>("proxyUsername", "")));
    proxy_user_layout->addWidget(_proxy_user_edit);
    proxy_layout->addLayout(proxy_user_layout);

    auto* proxy_pass_layout = new QHBoxLayout();
    proxy_pass_layout->addWidget(new QLabel("代理密码:", _scroll_widget));
    _proxy_pass_edit = new QLineEdit(_scroll_widget);
    _proxy_pass_edit->setEchoMode(QLineEdit::Password);
    _proxy_pass_edit->setText(QString::fromStdString(
        ConfigManager::get_setting<std::string>("proxyPassword", "")));
    proxy_pass_layout->addWidget(_proxy_pass_edit);
    proxy_layout->addLayout(proxy_pass_layout);

    _expand_layout->addWidget(proxy_group);

    // ---- 调试组 ----
    auto* debug_group = new QGroupBox("调试", _scroll_widget);
    auto* debug_layout = new QVBoxLayout(debug_group);

    auto* log_level_layout = new QHBoxLayout();
    log_level_layout->addWidget(new QLabel("日志等级:", _scroll_widget));
    _log_level_combo = new QComboBox(_scroll_widget);
    auto levels = get_level_names();
    for (auto& l : levels) {
        _log_level_combo->addItem(QString::fromStdString(l));
    }
    std::string saved_level = ConfigManager::get_setting<std::string>("logLevel", "DEBUG");
    int idx = _log_level_combo->findText(QString::fromStdString(saved_level));
    if (idx >= 0) _log_level_combo->setCurrentIndex(idx);
    log_level_layout->addWidget(_log_level_combo);
    debug_layout->addLayout(log_level_layout);

    auto* open_log_btn = new QPushButton("打开日志文件", _scroll_widget);
    connect(open_log_btn, &QPushButton::clicked, []() { open_log_file(); });
    debug_layout->addWidget(open_log_btn);

    _expand_layout->addWidget(debug_group);

    // ---- 关于组 ----
    auto* about_group = new QGroupBox("关于", _scroll_widget);
    auto* about_layout = new QVBoxLayout(about_group);

    auto* about_label = new QLabel(
        QString("123pan v%1  © Copyright %2").arg(VERSION).arg(YEAR), _scroll_widget);
    about_layout->addWidget(about_label);

    auto* project_btn = new QPushButton("项目页面", _scroll_widget);
    connect(project_btn, &QPushButton::clicked, []() {
        QDesktopServices::openUrl(QUrl(ABOUT_URL));
    });
    about_layout->addWidget(project_btn);

    auto* check_ver_btn = new QPushButton("检查更新", _scroll_widget);
    connect(check_ver_btn, &QPushButton::clicked,
            this, &SettingInterface::on_check_version);
    about_layout->addWidget(check_ver_btn);

    _expand_layout->addWidget(about_group);

    _expand_layout->addStretch();

    setWidget(_scroll_widget);

    // Connect signals
    connect(_download_folder_card, &QPushButton::clicked,
            this, &SettingInterface::on_download_folder_clicked);
    connect(_ask_download_check, &QCheckBox::toggled,
            this, &SettingInterface::on_ask_download_location_changed);
    connect(_multi_thread_check, &QCheckBox::toggled,
            this, &SettingInterface::on_multi_thread_changed);
    connect(_download_speed_card, &SpinBoxCard::value_changed,
            this, &SettingInterface::on_download_speed_changed);
    connect(_upload_speed_card, &SpinBoxCard::value_changed,
            this, &SettingInterface::on_upload_speed_changed);

    connect(_proxy_enabled_check, &QCheckBox::toggled,
            this, &SettingInterface::on_proxy_enabled_changed);
    connect(_proxy_type_combo, &QComboBox::currentTextChanged,
            this, &SettingInterface::on_proxy_type_changed);
    connect(_proxy_host_edit, &QLineEdit::editingFinished,
            this, &SettingInterface::on_proxy_host_changed);
    connect(_proxy_port_spin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &SettingInterface::on_proxy_port_changed);
    connect(_proxy_user_edit, &QLineEdit::editingFinished,
            this, &SettingInterface::on_proxy_user_changed);
    connect(_proxy_pass_edit, &QLineEdit::editingFinished,
            this, &SettingInterface::on_proxy_pass_changed);

    connect(_log_level_combo, &QComboBox::currentTextChanged,
            this, &SettingInterface::on_log_level_changed);
}

void SettingInterface::set_pan(std::shared_ptr<Pan123> pan) {
    _pan = std::move(pan);
}

// ---- Event handlers ----
void SettingInterface::on_download_folder_clicked() {
    QString folder = QFileDialog::getExistingDirectory(this, "选择下载文件夹", "./");
    if (folder.isEmpty()) return;
    _download_folder_card->setToolTip("当前下载目录: " + folder);
    ConfigManager::set_setting("defaultDownloadPath", folder.toStdString());
    auto logger = get_logger("ui");
    logger->info("下载目录变更为: {}", folder.toStdString());
}

void SettingInterface::on_ask_download_location_changed(bool checked) {
    ConfigManager::set_setting("askDownloadLocation", checked);
}

void SettingInterface::on_multi_thread_changed(bool checked) {
    ConfigManager::set_setting("multiThreadDownload", checked);
    if (_pan) _pan->set_download_multi_thread(checked);
}

void SettingInterface::on_download_speed_changed(int val) {
    ConfigManager::set_setting("downloadSpeedLimit", val);
    if (_pan) _pan->set_download_speed_limit(val);
}

void SettingInterface::on_upload_speed_changed(int val) {
    ConfigManager::set_setting("uploadSpeedLimit", val);
    if (_pan) _pan->set_upload_speed_limit(val);
}

void SettingInterface::apply_proxy_to_service() {
    if (!_pan) return;
    bool enabled = ConfigManager::get_setting<bool>("proxyEnabled", false);
    if (enabled) {
        std::string proxy_type = ConfigManager::get_setting<std::string>("proxyType", "http");
        std::string host = ConfigManager::get_setting<std::string>("proxyHost", "");
        int port = ConfigManager::get_setting<int>("proxyPort", 0);
        if (!host.empty() && port > 0) {
            std::string username = ConfigManager::get_setting<std::string>("proxyUsername", "");
            std::string password = ConfigManager::get_setting<std::string>("proxyPassword", "");
            _pan->set_download_proxy(proxy_type, host, port, username, password);
            return;
        }
    }
    _pan->clear_download_proxy();
}

void SettingInterface::on_proxy_enabled_changed(bool checked) {
    ConfigManager::set_setting("proxyEnabled", checked);
    apply_proxy_to_service();
}

void SettingInterface::on_proxy_type_changed(const QString& text) {
    std::string pt = (text == "SOCKS5") ? "socks5" : "http";
    ConfigManager::set_setting("proxyType", pt);
    apply_proxy_to_service();
}

void SettingInterface::on_proxy_host_changed() {
    ConfigManager::set_setting("proxyHost", _proxy_host_edit->text().toStdString());
    apply_proxy_to_service();
}

void SettingInterface::on_proxy_port_changed(int val) {
    ConfigManager::set_setting("proxyPort", val);
    apply_proxy_to_service();
}

void SettingInterface::on_proxy_user_changed() {
    ConfigManager::set_setting("proxyUsername", _proxy_user_edit->text().toStdString());
    apply_proxy_to_service();
}

void SettingInterface::on_proxy_pass_changed() {
    ConfigManager::set_setting("proxyPassword", _proxy_pass_edit->text().toStdString());
    apply_proxy_to_service();
}

void SettingInterface::on_log_level_changed(const QString& level_name) {
    std::string ln = level_name.toStdString();
    ConfigManager::set_setting("logLevel", ln);
    set_log_level(ln);
}

void SettingInterface::on_check_version() {
    if (check_version()) {
        QMessageBox::information(this, "检查更新", "当前是最新版本");
    } else {
        QMessageBox::information(this, "检查更新", "当前不是最新版本，或无法完成检查");
    }
}

}  // namespace app
