#include "app/qml/Backend.h"
#include "app/common/api.hpp"
#include "app/common/config.hpp"
#include "app/common/credential.hpp"
#include "app/common/const.hpp"
#include "app/common/log.hpp"

#include <QStandardItemModel>
#include <QDesktopServices>
#include <QUrl>
#include <QFileDialog>

namespace app {

// ---- Helper: simple list model for QML ----
struct ListRow {
    QString name;
    QString size;
    int type = 0;
    int progress = 0;
    QString status;
    qint64 fileId = 0;
};

class SimpleListModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        SizeRole,
        TypeRole,
        ProgressRole,
        StatusRole,
        FileIdRole,
    };

    explicit SimpleListModel(QObject* parent = nullptr) : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex& = {}) const override { return static_cast<int>(_data.size()); }
    QVariant data(const QModelIndex& index, int role) const override {
        if (index.row() < 0 || index.row() >= static_cast<int>(_data.size())) return {};
        const auto& row = _data[index.row()];
        switch (role) {
        case NameRole:     return row.name;
        case SizeRole:     return row.size;
        case TypeRole:     return row.type;
        case ProgressRole: return row.progress;
        case StatusRole:   return row.status;
        case FileIdRole:   return row.fileId;
        default:           return {};
        }
    }
    QHash<int, QByteArray> roleNames() const override {
        return {
            {NameRole, "name"},
            {SizeRole, "size"},
            {TypeRole, "type"},
            {ProgressRole, "progress"},
            {StatusRole, "status"},
            {FileIdRole, "fileId"},
        };
    }

    void clear() {
        beginResetModel();
        _data.clear();
        endResetModel();
    }

    void addRow(const ListRow& row) {
        beginInsertRows(QModelIndex(), static_cast<int>(_data.size()), static_cast<int>(_data.size()));
        _data.append(row);
        endInsertRows();
    }

    void setRows(const QVector<ListRow>& rows) {
        beginResetModel();
        _data = rows;
        endResetModel();
    }

    const ListRow& rowAt(int i) const { return _data[i]; }

private:
    QVector<ListRow> _data;
};

// ============================================================
// Backend
// ============================================================
Backend::Backend(QObject* parent)
    : QObject(parent)
{
    _fileTableModel = new SimpleListModel(this);
    _fileTreeModel = new SimpleListModel(this);
    _uploadTaskModel = new SimpleListModel(this);
    _downloadTaskModel = new SimpleListModel(this);
}

Backend::~Backend() = default;

void Backend::init() {
    auto logger = get_logger("backend");
    logger->info("Backend 初始化");

    // Try auto-login with saved credentials
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
        if (!saved.empty()) {
            _pan = std::make_shared<Pan123>(true, account.toStdString(), password.toStdString());
        } else {
            _pan = std::make_shared<Pan123>(false, account.toStdString(), password.toStdString());
        }

        int code = _pan->login();
        if (code != 200 && code != 0) {
            emit loginFailed(QString("登录失败，返回码: %1").arg(code));
            return;
        }

        try { _pan->save_file(); } catch (...) {}

        _isLoggedIn = true;
        loadSettings();
        emit loginSuccess();
    } catch (const std::exception& e) {
        emit loginFailed(QString::fromStdString(e.what()));
    }
}

void Backend::logout() {
    ConfigManager::set_current_account("");
    _isLoggedIn = false;
    _pan.reset();
    emit logoutRequested();
}

void Backend::switchAccount() {
    _isLoggedIn = false;
    _pan.reset();
    emit logoutRequested();
}

// ---- 用户信息 ----
QString Backend::currentUserName() const {
    return _pan ? QString::fromStdString(_pan->user_name) : QString();
}
QString Backend::currentDeviceInfo() const {
    if (!_pan) return {};
    return QString("%1 / %2").arg(QString::fromStdString(_pan->devicetype),
                                  QString::fromStdString(_pan->osversion));
}

// ---- 下载设置 ----
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
        bool mt = ConfigManager::get_setting<bool>("multiThreadDownload", true);
        int dl = ConfigManager::get_setting<int>("downloadSpeedLimit", 0);
        int ul = ConfigManager::get_setting<int>("uploadSpeedLimit", 0);
        _pan->set_download_multi_thread(mt);
        _pan->set_download_speed_limit(dl);
        _pan->set_upload_speed_limit(ul);
    }
    applyProxyToService();
}

QString Backend::downloadPath() const {
    return QString::fromStdString(
        ConfigManager::get_setting<std::string>("defaultDownloadPath", ""));
}
void Backend::setDownloadPath(const QString& path) {
    ConfigManager::set_setting("defaultDownloadPath", path.toStdString());
    emit downloadPathChanged();
}
bool Backend::askDownloadLocation() const {
    return ConfigManager::get_setting<bool>("askDownloadLocation", true);
}
void Backend::setAskDownloadLocation(bool v) {
    ConfigManager::set_setting("askDownloadLocation", v);
    emit askDownloadLocationChanged();
}
bool Backend::multiThreadDownload() const {
    return ConfigManager::get_setting<bool>("multiThreadDownload", true);
}
void Backend::setMultiThreadDownload(bool v) {
    ConfigManager::set_setting("multiThreadDownload", v);
    if (_pan) _pan->set_download_multi_thread(v);
    emit multiThreadDownloadChanged();
}
int Backend::downloadSpeedLimit() const {
    return ConfigManager::get_setting<int>("downloadSpeedLimit", 0);
}
void Backend::setDownloadSpeedLimit(int v) {
    ConfigManager::set_setting("downloadSpeedLimit", v);
    if (_pan) _pan->set_download_speed_limit(v);
    emit downloadSpeedLimitChanged();
}
int Backend::uploadSpeedLimit() const {
    return ConfigManager::get_setting<int>("uploadSpeedLimit", 0);
}
void Backend::setUploadSpeedLimit(int v) {
    ConfigManager::set_setting("uploadSpeedLimit", v);
    if (_pan) _pan->set_upload_speed_limit(v);
    emit uploadSpeedLimitChanged();
}
void Backend::selectDownloadFolder() {
    // This requires a QWidget parent; in QML we'd use a FileDialog
    // For now, just use the native approach
}

// ---- 代理设置 ----
bool Backend::proxyEnabled() const {
    return ConfigManager::get_setting<bool>("proxyEnabled", false);
}
void Backend::setProxyEnabled(bool v) {
    ConfigManager::set_setting("proxyEnabled", v);
    applyProxyToService();
    emit proxyEnabledChanged();
}
QString Backend::proxyType() const {
    return QString::fromStdString(ConfigManager::get_setting<std::string>("proxyType", "http"));
}
void Backend::setProxyType(const QString& v) {
    ConfigManager::set_setting("proxyType", v.toStdString());
    applyProxyToService();
    emit proxyTypeChanged();
}
QString Backend::proxyHost() const {
    return QString::fromStdString(ConfigManager::get_setting<std::string>("proxyHost", ""));
}
void Backend::setProxyHost(const QString& v) {
    ConfigManager::set_setting("proxyHost", v.toStdString());
    applyProxyToService();
    emit proxyHostChanged();
}
int Backend::proxyPort() const {
    return ConfigManager::get_setting<int>("proxyPort", 0);
}
void Backend::setProxyPort(int v) {
    ConfigManager::set_setting("proxyPort", v);
    applyProxyToService();
    emit proxyPortChanged();
}
void Backend::applyProxyToService() {
    if (!_pan) return;
    if (proxyEnabled()) {
        std::string pt = proxyType().toStdString();
        std::string host = proxyHost().toStdString();
        int port = proxyPort();
        if (!host.empty() && port > 0) {
            _pan->set_download_proxy(pt, host, port);
            return;
        }
    }
    _pan->clear_download_proxy();
}

// ---- 调试 ----
QStringList Backend::logLevels() const {
    auto levels = get_level_names();
    QStringList result;
    for (auto& l : levels) result << QString::fromStdString(l);
    return result;
}
int Backend::logLevelIndex() const {
    auto levels = get_level_names();
    auto current = get_current_level_name();
    for (size_t i = 0; i < levels.size(); ++i) {
        if (levels[i] == current) return static_cast<int>(i);
    }
    return 0;
}
void Backend::setLogLevelIndex(int idx) {
    auto levels = get_level_names();
    if (idx >= 0 && idx < static_cast<int>(levels.size())) {
        set_log_level(levels[idx]);
        ConfigManager::set_setting("logLevel", levels[idx]);
        emit logLevelIndexChanged();
    }
}
void Backend::openLogFile() {
    app::open_log_file();
}

// ---- 关于 ----
QString Backend::appVersion() const {
    return QString::fromUtf8(VERSION);
}
void Backend::openProjectPage() {
    QDesktopServices::openUrl(QUrl(QString::fromUtf8(ABOUT_URL)));
}
void Backend::checkUpdate() {
    app::check_version();
}

// ---- 文件浏览 ----
QVariantList Backend::fileBreadcrumb() const {
    QVariantList list;
    if (!_pan) return list;

    // Build breadcrumb from file list navigation state
    // Simplification: return single root + current dir
    QVariantMap root;
    root["title"] = "根目录";
    root["data"] = 0;
    list.append(root);

    if (_fileCurrentDirId != 0) {
        QVariantMap cur;
        cur["title"] = QString("目录 %1").arg(_fileCurrentDirId);
        cur["data"] = _fileCurrentDirId;
        list.append(cur);
    }
    return list;
}

void Backend::refreshFileList() {
    if (!_pan) return;

    auto [code, items] = _pan->get_dir_by_id(_fileCurrentDirId, false, true, 100);
    if (code != 0) return;

    QVector<ListRow> tableRows;
    QVector<ListRow> treeRows;

    for (auto& item : items) {
        ListRow row;
        std::string name = item.value("FileName", item.value("fileName", ""));
        row.type = item.value("Type", item.value("type", 0));
        int64_t size = item.value("Size", item.value("size", 0));
        int64_t fid = item.value("FileId", item.value("fileId", 0));

        row.name = QString::fromStdString(name);
        row.size = QString::fromStdString(format_file_size(size));
        row.fileId = static_cast<qint64>(fid);

        if (row.type == 1) {
            row.name = "📁 " + row.name;
            treeRows.append(row);
        }
        tableRows.append(row);
    }

    static_cast<SimpleListModel*>(_fileTableModel)->setRows(tableRows);
    static_cast<SimpleListModel*>(_fileTreeModel)->setRows(treeRows);
    emit fileListChanged();
}

void Backend::fileNavigateTo(int dirId) {
    _fileCurrentDirId = dirId;
    if (_pan) {
        _pan->parent_file_id = dirId;
        _pan->file_page = 0;
    }
    refreshFileList();
}

void Backend::fileGoParentDir() {
    if (!_pan || _fileCurrentDirId == 0) return;
    _fileCurrentDirId = 0;
    _pan->parent_file_id = 0;
    _pan->file_page = 0;
    refreshFileList();
}

void Backend::fileRefresh() {
    if (_pan) _pan->file_page = 0;
    refreshFileList();
}

void Backend::fileCreateFolder() {
    // TODO: Show a dialog in QML to get folder name
    if (!_pan) return;
    // _pan->mkdir("new_folder");
    // refreshFileList();
}

void Backend::fileUpload() {
    // TODO: Use QML FileDialog or native dialog
}

void Backend::fileDownload() {
    // TODO: Get selected file from table and add download task
}

void Backend::fileDelete() {
    // TODO: Get selected file and delete
}

void Backend::fileOpenItem(int row) {
    auto* model = static_cast<SimpleListModel*>(_fileTableModel);
    auto idx = model->index(row);
    int type = model->data(idx, SimpleListModel::TypeRole).toInt();
    int64_t fid = model->data(idx, SimpleListModel::FileIdRole).toLongLong();

    if (type == 1) {
        // Navigate into folder
        fileNavigateTo(static_cast<int>(fid));
    } else {
        // Download file
        fileDownload();
    }
}

}  // namespace app

#include "Backend.moc"
