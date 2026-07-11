#pragma once

#include <QObject>
#include <QAbstractListModel>
#include <QVariantList>
#include <QStringList>
#include <memory>
#include <vector>
#include <string>

namespace app {

class Pan123;

/// C++/QML 桥接后端。
/// 将所有业务逻辑暴露为 Q_PROPERTY 和 Q_INVOKABLE 方法，供 QML 直接调用。
class Backend : public QObject {
    Q_OBJECT

    // ---- 登录状态 ----
    Q_PROPERTY(bool isLoggedIn READ isLoggedIn NOTIFY loginSuccess)

    // ---- 当前用户 ----
    Q_PROPERTY(QString currentUserName READ currentUserName NOTIFY loginSuccess)
    Q_PROPERTY(QString currentDeviceInfo READ currentDeviceInfo NOTIFY loginSuccess)

    // ---- 下载设置 ----
    Q_PROPERTY(QString downloadPath READ downloadPath WRITE setDownloadPath NOTIFY downloadPathChanged)
    Q_PROPERTY(bool askDownloadLocation READ askDownloadLocation WRITE setAskDownloadLocation NOTIFY askDownloadLocationChanged)
    Q_PROPERTY(bool multiThreadDownload READ multiThreadDownload WRITE setMultiThreadDownload NOTIFY multiThreadDownloadChanged)
    Q_PROPERTY(int downloadSpeedLimit READ downloadSpeedLimit WRITE setDownloadSpeedLimit NOTIFY downloadSpeedLimitChanged)
    Q_PROPERTY(int uploadSpeedLimit READ uploadSpeedLimit WRITE setUploadSpeedLimit NOTIFY uploadSpeedLimitChanged)

    // ---- 代理设置 ----
    Q_PROPERTY(bool proxyEnabled READ proxyEnabled WRITE setProxyEnabled NOTIFY proxyEnabledChanged)
    Q_PROPERTY(QString proxyType READ proxyType WRITE setProxyType NOTIFY proxyTypeChanged)
    Q_PROPERTY(QString proxyHost READ proxyHost WRITE setProxyHost NOTIFY proxyHostChanged)
    Q_PROPERTY(int proxyPort READ proxyPort WRITE setProxyPort NOTIFY proxyPortChanged)

    // ---- 调试 ----
    Q_PROPERTY(QStringList logLevels READ logLevels CONSTANT)
    Q_PROPERTY(int logLevelIndex READ logLevelIndex WRITE setLogLevelIndex NOTIFY logLevelIndexChanged)

    // ---- 关于 ----
    Q_PROPERTY(QString appVersion READ appVersion CONSTANT)

    // ---- 文件浏览 ----
    Q_PROPERTY(int fileCurrentDirId READ fileCurrentDirId NOTIFY fileListChanged)
    Q_PROPERTY(QVariantList fileBreadcrumb READ fileBreadcrumb NOTIFY fileListChanged)
    Q_PROPERTY(QObject* fileTableModel READ fileTableModel CONSTANT)
    Q_PROPERTY(QObject* fileTreeModel READ fileTreeModel CONSTANT)

    // ---- 传输 ----
    Q_PROPERTY(QObject* uploadTaskModel READ uploadTaskModel CONSTANT)
    Q_PROPERTY(QObject* downloadTaskModel READ downloadTaskModel CONSTANT)

public:
    explicit Backend(QObject* parent = nullptr);
    ~Backend() override;

    // ---- 初始化 ----
    Q_INVOKABLE void init();

    // ---- 登录 ----
    Q_INVOKABLE void login(const QString& account, const QString& password);
    Q_INVOKABLE void logout();
    Q_INVOKABLE void switchAccount();

    bool isLoggedIn() const { return _isLoggedIn; }

    // ---- 用户信息 ----
    QString currentUserName() const;
    QString currentDeviceInfo() const;

    // ---- 下载设置 ----
    QString downloadPath() const;
    void setDownloadPath(const QString& path);
    bool askDownloadLocation() const;
    void setAskDownloadLocation(bool v);
    bool multiThreadDownload() const;
    void setMultiThreadDownload(bool v);
    int downloadSpeedLimit() const;
    void setDownloadSpeedLimit(int v);
    int uploadSpeedLimit() const;
    void setUploadSpeedLimit(int v);
    Q_INVOKABLE void selectDownloadFolder();

    // ---- 代理设置 ----
    bool proxyEnabled() const;
    void setProxyEnabled(bool v);
    QString proxyType() const;
    void setProxyType(const QString& v);
    QString proxyHost() const;
    void setProxyHost(const QString& v);
    int proxyPort() const;
    void setProxyPort(int v);

    // ---- 调试 ----
    QStringList logLevels() const;
    int logLevelIndex() const;
    void setLogLevelIndex(int idx);
    Q_INVOKABLE void openLogFile();

    // ---- 关于 ----
    QString appVersion() const;
    Q_INVOKABLE void openProjectPage();
    Q_INVOKABLE void checkUpdate();

    // ---- 文件浏览 ----
    int fileCurrentDirId() const { return _fileCurrentDirId; }
    QVariantList fileBreadcrumb() const;
    QObject* fileTableModel() { return _fileTableModel; }
    QObject* fileTreeModel() { return _fileTreeModel; }

    Q_INVOKABLE void fileNavigateTo(int dirId);
    Q_INVOKABLE void fileGoParentDir();
    Q_INVOKABLE void fileRefresh();
    Q_INVOKABLE void fileCreateFolder();
    Q_INVOKABLE void fileUpload();
    Q_INVOKABLE void fileDownload();
    Q_INVOKABLE void fileDelete();
    Q_INVOKABLE void fileOpenItem(int row);

    // ---- 传输 ----
    QObject* uploadTaskModel() { return _uploadTaskModel; }
    QObject* downloadTaskModel() { return _downloadTaskModel; }

signals:
    void loginSuccess();
    void loginFailed(const QString& message);
    void logoutRequested();

    void downloadPathChanged();
    void askDownloadLocationChanged();
    void multiThreadDownloadChanged();
    void downloadSpeedLimitChanged();
    void uploadSpeedLimitChanged();

    void proxyEnabledChanged();
    void proxyTypeChanged();
    void proxyHostChanged();
    void proxyPortChanged();

    void logLevelIndexChanged();

    void fileListChanged();
    void storageInfoChanged(const QString& usedText);

private:
    void loadSettings();
    void applyProxyToService();
    void refreshFileList();

    std::shared_ptr<Pan123> _pan;
    bool _isLoggedIn = false;

    // File browsing state
    int _fileCurrentDirId = 0;
    QObject* _fileTableModel;
    QObject* _fileTreeModel;

    // Transfer state
    QObject* _uploadTaskModel;
    QObject* _downloadTaskModel;
};

}  // namespace app
