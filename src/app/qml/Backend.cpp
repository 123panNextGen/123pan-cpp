#include "app/qml/Backend.h"
#include "app/common/api.hpp"
#include "app/common/config.hpp"
#include "app/common/credential.hpp"
#include "app/common/const.hpp"
#include "app/common/log.hpp"

#include <QDesktopServices>
#include <QUrl>

namespace app {

Backend::Backend(QObject* parent) : QObject(parent) {}
Backend::~Backend() = default;

void Backend::init() {
    auto logger = get_logger("backend");
    logger->info("Backend 初始化");

    std::string currentAccount = ConfigManager::get_current_account_name();
    if (!currentAccount.empty()) {
        json account = ConfigManager::get_account(currentAccount);
        std::string pwd = account.value("passWord", "");
        if (!pwd.empty()) {
            if (pwd.starts_with("enc:")) pwd = decrypt_credential(pwd);
            if (!pwd.empty()) {
                try {
                    _pan = std::make_shared<Pan123>(true, currentAccount, pwd);
                    auto [code, items] = _pan->get_dir(false);
                    if (code == 0) {
                        _isLoggedIn = true;
                        loadSettings();
                        fileRefresh();
                        emit loginSuccess();
                        return;
                    }
                } catch (...) {}
            }
        }
    }
}

void Backend::login(const QString& account, const QString& password) {
    auto logger = get_logger("backend");
    logger->info("登录: {}", account.toStdString());

    try {
        json saved = ConfigManager::get_account(account.toStdString());
        if (!saved.empty())
            _pan = std::make_shared<Pan123>(true, account.toStdString(), password.toStdString());
        else
            _pan = std::make_shared<Pan123>(false, account.toStdString(), password.toStdString());

        int code = _pan->login();
        if (code != 200 && code != 0) {
            emit loginFailed(QString("登录失败，返回码: %1").arg(code));
            return;
        }
        try { _pan->save_file(); } catch (...) {}
        _isLoggedIn = true;
        loadSettings();
        fileRefresh();
        emit loginSuccess();
    } catch (const std::exception& e) {
        emit loginFailed(QString::fromStdString(e.what()));
    }
}

void Backend::logout() {
    ConfigManager::set_current_account("");
    _isLoggedIn = false;
    _pan.reset();
    _fileTableData.clear();
    _fileTreeData.clear();
    emit logoutRequested();
}

void Backend::switchAccount() {
    _isLoggedIn = false;
    _pan.reset();
    emit logoutRequested();
}

QString Backend::currentUserName() const {
    return _pan ? QString::fromStdString(_pan->user_name) : QString();
}
QString Backend::currentDeviceInfo() const {
    if (!_pan) return {};
    return QString("%1 / %2")
        .arg(QString::fromStdString(_pan->devicetype),
             QString::fromStdString(_pan->osversion));
}

// ---- 设置 ----
void Backend::loadSettings() {
    emit downloadPathChanged();
    emit askDownloadLocationChanged();
    emit multiThreadDownloadChanged();
    emit downloadSpeedLimitChanged();
    emit uploadSpeedLimitChanged();
    emit proxyEnabledChanged();
    emit proxyTypeChanged();
    emit proxyHostChanged();
    emit proxyPortChanged();
    emit logLevelIndexChanged();
    if (_pan) {
        _pan->set_download_multi_thread(multiThreadDownload());
        _pan->set_download_speed_limit(downloadSpeedLimit());
        _pan->set_upload_speed_limit(uploadSpeedLimit());
    }
    applyProxyToService();
}

QString Backend::downloadPath() const {
    return QString::fromStdString(ConfigManager::get_setting<std::string>("defaultDownloadPath", ""));
}
void Backend::setDownloadPath(const QString& p) { ConfigManager::set_setting("defaultDownloadPath", p.toStdString()); emit downloadPathChanged(); }
bool Backend::askDownloadLocation() const { return ConfigManager::get_setting<bool>("askDownloadLocation", true); }
void Backend::setAskDownloadLocation(bool v) { ConfigManager::set_setting("askDownloadLocation", v); emit askDownloadLocationChanged(); }
bool Backend::multiThreadDownload() const { return ConfigManager::get_setting<bool>("multiThreadDownload", true); }
void Backend::setMultiThreadDownload(bool v) { ConfigManager::set_setting("multiThreadDownload", v); if (_pan) _pan->set_download_multi_thread(v); emit multiThreadDownloadChanged(); }
int Backend::downloadSpeedLimit() const { return ConfigManager::get_setting<int>("downloadSpeedLimit", 0); }
void Backend::setDownloadSpeedLimit(int v) { ConfigManager::set_setting("downloadSpeedLimit", v); if (_pan) _pan->set_download_speed_limit(v); emit downloadSpeedLimitChanged(); }
int Backend::uploadSpeedLimit() const { return ConfigManager::get_setting<int>("uploadSpeedLimit", 0); }
void Backend::setUploadSpeedLimit(int v) { ConfigManager::set_setting("uploadSpeedLimit", v); if (_pan) _pan->set_upload_speed_limit(v); emit uploadSpeedLimitChanged(); }

bool Backend::proxyEnabled() const { return ConfigManager::get_setting<bool>("proxyEnabled", false); }
void Backend::setProxyEnabled(bool v) { ConfigManager::set_setting("proxyEnabled", v); applyProxyToService(); emit proxyEnabledChanged(); }
QString Backend::proxyType() const { return QString::fromStdString(ConfigManager::get_setting<std::string>("proxyType", "http")); }
void Backend::setProxyType(const QString& v) { ConfigManager::set_setting("proxyType", v.toStdString()); applyProxyToService(); emit proxyTypeChanged(); }
QString Backend::proxyHost() const { return QString::fromStdString(ConfigManager::get_setting<std::string>("proxyHost", "")); }
void Backend::setProxyHost(const QString& v) { ConfigManager::set_setting("proxyHost", v.toStdString()); applyProxyToService(); emit proxyHostChanged(); }
int Backend::proxyPort() const { return ConfigManager::get_setting<int>("proxyPort", 0); }
void Backend::setProxyPort(int v) { ConfigManager::set_setting("proxyPort", v); applyProxyToService(); emit proxyPortChanged(); }
void Backend::applyProxyToService() {
    if (!_pan) return;
    if (proxyEnabled()) {
        auto h = proxyHost().toStdString();
        int p = proxyPort();
        if (!h.empty() && p > 0) {
            _pan->set_download_proxy(proxyType().toStdString(), h, p);
            return;
        }
    }
    _pan->clear_download_proxy();
}

QStringList Backend::logLevels() const {
    QStringList r; for (auto& l : get_level_names()) r << QString::fromStdString(l); return r;
}
int Backend::logLevelIndex() const {
    auto levels = get_level_names(); auto cur = get_current_level_name();
    for (size_t i = 0; i < levels.size(); ++i) if (levels[i] == cur) return (int)i;
    return 0;
}
void Backend::setLogLevelIndex(int idx) {
    auto levels = get_level_names();
    if (idx >= 0 && idx < (int)levels.size()) { set_log_level(levels[idx]); ConfigManager::set_setting("logLevel", levels[idx]); emit logLevelIndexChanged(); }
}
void Backend::openLogFile() { app::open_log_file(); }
QString Backend::appVersion() const { return QString::fromUtf8(VERSION); }
void Backend::openProjectPage() { QDesktopServices::openUrl(QUrl(QString::fromUtf8(ABOUT_URL))); }
void Backend::checkUpdate() { app::check_version(); }

// ---- 列定义 ----
QVariantList Backend::fileTableColumns() const {
    return {
        QVariantMap{{"title", "名称"}, {"dataIndex", "name"}, {"width", 280}},
        QVariantMap{{"title", "类型"}, {"dataIndex", "typeName"}, {"width", 120}},
        QVariantMap{{"title", "大小"}, {"dataIndex", "size"}, {"width", 100}},
    };
}
QVariantList Backend::transferColumns() const {
    return {
        QVariantMap{{"title", "文件名"}, {"dataIndex", "name"}, {"width", 240}},
        QVariantMap{{"title", "大小"}, {"dataIndex", "size"}, {"width", 100}},
        QVariantMap{{"title", "进度"}, {"dataIndex", "progress"}, {"width", 80}},
        QVariantMap{{"title", "状态"}, {"dataIndex", "status"}, {"width", 80}},
    };
}

// ---- 文件列表 ----
void Backend::refreshFileList() {
    if (!_pan) return;
    _pan->parent_file_id = _fileCurrentDirId;
    _pan->file_page = 0;
    auto [code, items] = _pan->get_dir_by_id(_fileCurrentDirId, false, true, 100);
    if (code != 0) { emit fileListChanged(); return; }

    _fileTableData.clear();
    _fileTreeData.clear();
    for (auto& item : items) {
        std::string name = item.value("FileName", item.value("fileName", ""));
        int type = item.value("Type", item.value("type", 0));
        int64_t sz = item.value("Size", item.value("size", 0));
        int64_t fid = item.value("FileId", item.value("fileId", 0));
        QVariantMap row;
        row["name"] = QString::fromStdString(name);
        row["typeName"] = type == 1 ? "📁 文件夹" : QString::fromStdString(get_file_type_name(type));
        row["size"] = QString::fromStdString(format_file_size(sz));
        row["type"] = type;
        row["fileId"] = (qint64)fid;
        if (type == 1) _fileTreeData.append(row);
        _fileTableData.append(row);
    }
    emit fileListChanged();
}

void Backend::fileNavigateTo(int dirId) { _fileCurrentDirId = dirId; refreshFileList(); }
void Backend::fileGoParentDir() { if (_fileCurrentDirId != 0) { _fileCurrentDirId = 0; refreshFileList(); } }
void Backend::fileRefresh() { refreshFileList(); }

void Backend::fileCreateFolder(const QString& name) {
    if (!_pan || name.isEmpty()) return;
    try { _pan->mkdir(name.toStdString()); refreshFileList(); } catch (...) {}
}

void Backend::fileDeleteItem(int fileId) {
    if (!_pan) return;
    for (size_t i = 0; i < _pan->list.size(); ++i) {
        int64_t fid = _pan->list[i].value("FileId", _pan->list[i].value("fileId", 0));
        if (fid == fileId) {
            _pan->delete_file((int)i, true, true);
            refreshFileList();
            return;
        }
    }
}

void Backend::fileRenameItem(int fileId, const QString& newName) {
    if (!_pan) return;
    try { _pan->rename_file(fileId, newName.toStdString()); refreshFileList(); } catch (...) {}
}

void Backend::fileDownloadItem(int fileId) {
    if (!_pan) return;
    for (auto& item : _pan->list) {
        int64_t fid = item.value("FileId", item.value("fileId", 0));
        if (fid != fileId) continue;
        std::string name = item.value("FileName", item.value("fileName", ""));
        int64_t sz = item.value("Size", item.value("size", 0));
        QVariantMap task;
        task["name"] = QString::fromStdString(name);
        task["size"] = QString::fromStdString(format_file_size(sz));
        task["progress"] = "0%";
        task["status"] = "已添加";
        _downloadData.append(task);
        emit transferChanged();
        return;
    }
}

}  // namespace app

#include "Backend.moc"
