#pragma once
#include <QObject>
#include <QVariantList>
#include <QStringList>
#include <memory>
#include <vector>
#include <string>

namespace app { class Pan123; }
namespace app {

class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool isLoggedIn READ isLoggedIn NOTIFY loginSuccess)
    Q_PROPERTY(QString currentUserName READ currentUserName NOTIFY loginSuccess)
    Q_PROPERTY(QString currentDeviceInfo READ currentDeviceInfo NOTIFY loginSuccess)
    Q_PROPERTY(int fileCurrentDirId READ fileCurrentDirId NOTIFY fileListChanged)
    Q_PROPERTY(QVariantList fileTableDataSource READ fileTableDataSource NOTIFY fileListChanged)
    Q_PROPERTY(QVariantList fileTableColumns READ fileTableColumns CONSTANT)
    Q_PROPERTY(QVariantList fileTreeDataSource READ fileTreeDataSource NOTIFY fileListChanged)
    Q_PROPERTY(QVariantList fileBreadcrumb READ fileBreadcrumb NOTIFY fileListChanged)
    Q_PROPERTY(QString storageUsed READ storageUsed NOTIFY storageChanged)
    Q_PROPERTY(QString storageTotal READ storageTotal CONSTANT)
    Q_PROPERTY(double storagePercent READ storagePercent NOTIFY storageChanged)
    Q_PROPERTY(QVariantList uploadDataSource READ uploadDataSource NOTIFY transferChanged)
    Q_PROPERTY(QVariantList downloadDataSource READ downloadDataSource NOTIFY transferChanged)
    Q_PROPERTY(QVariantList transferColumns READ transferColumns CONSTANT)
    Q_PROPERTY(QString downloadPath READ downloadPath WRITE setDownloadPath NOTIFY downloadPathChanged)
    Q_PROPERTY(bool askDownloadLocation READ askDownloadLocation WRITE setAskDownloadLocation NOTIFY askDownloadLocationChanged)
    Q_PROPERTY(bool multiThreadDownload READ multiThreadDownload WRITE setMultiThreadDownload NOTIFY multiThreadDownloadChanged)
    Q_PROPERTY(int downloadSpeedLimit READ downloadSpeedLimit WRITE setDownloadSpeedLimit NOTIFY downloadSpeedLimitChanged)
    Q_PROPERTY(int uploadSpeedLimit READ uploadSpeedLimit WRITE setUploadSpeedLimit NOTIFY uploadSpeedLimitChanged)
    Q_PROPERTY(bool proxyEnabled READ proxyEnabled WRITE setProxyEnabled NOTIFY proxyEnabledChanged)
    Q_PROPERTY(QString proxyType READ proxyType WRITE setProxyType NOTIFY proxyTypeChanged)
    Q_PROPERTY(QString proxyHost READ proxyHost WRITE setProxyHost NOTIFY proxyHostChanged)
    Q_PROPERTY(int proxyPort READ proxyPort WRITE setProxyPort NOTIFY proxyPortChanged)
    Q_PROPERTY(QStringList logLevels READ logLevels CONSTANT)
    Q_PROPERTY(int logLevelIndex READ logLevelIndex WRITE setLogLevelIndex NOTIFY logLevelIndexChanged)
    Q_PROPERTY(QString appVersion READ appVersion CONSTANT)

public:
    explicit Backend(QObject* parent = nullptr);
    ~Backend() override;

    Q_INVOKABLE void init();
    Q_INVOKABLE void login(const QString& a, const QString& p);
    Q_INVOKABLE void logout();
    Q_INVOKABLE void switchAccount();
    bool isLoggedIn() const { return _isLoggedIn; }
    QString currentUserName() const;
    QString currentDeviceInfo() const;

    // Settings
    QString downloadPath() const; void setDownloadPath(const QString& v);
    bool askDownloadLocation() const; void setAskDownloadLocation(bool v);
    bool multiThreadDownload() const; void setMultiThreadDownload(bool v);
    int downloadSpeedLimit() const; void setDownloadSpeedLimit(int v);
    int uploadSpeedLimit() const; void setUploadSpeedLimit(int v);
    bool proxyEnabled() const; void setProxyEnabled(bool v);
    QString proxyType() const; void setProxyType(const QString& v);
    QString proxyHost() const; void setProxyHost(const QString& v);
    int proxyPort() const; void setProxyPort(int v);
    QStringList logLevels() const; int logLevelIndex() const; void setLogLevelIndex(int idx);
    Q_INVOKABLE void openLogFile();
    Q_INVOKABLE void pickDownloadPath();
    QString appVersion() const;
    Q_INVOKABLE void openProjectPage(); Q_INVOKABLE void checkUpdate();

    // File
    int fileCurrentDirId() const { return _fileCurrentDirId; }
    QVariantList fileTableDataSource() const { return _fileTableData; }
    QVariantList fileTableColumns() const;
    QVariantList fileTreeDataSource() const { return _fileTreeData; }
    QVariantList fileBreadcrumb() const { return _breadcrumb; }
    QString storageUsed() const { return _storageUsed; }
    QString storageTotal() const;
    double storagePercent() const { return _storagePercent; }
    Q_INVOKABLE void fileRefresh();
    Q_INVOKABLE void fileGoParentDir();
    Q_INVOKABLE void fileNavigateByBreadcrumb(int index);
    Q_INVOKABLE void fileNavigateToDir(qint64 dirId);
    Q_INVOKABLE void fileCreateFolder(const QString& name);
    Q_INVOKABLE void fileDeleteItem(qint64 fileId);
    Q_INVOKABLE void fileRenameItem(qint64 fileId, const QString& newName);
    Q_INVOKABLE void fileDownloadItem(qint64 fileId);
    Q_INVOKABLE void fileShareItem(qint64 fileId);
    Q_INVOKABLE void fileCopyLink(qint64 fileId);
    Q_INVOKABLE void fileUploadItem();

    // Transfer
    QVariantList uploadDataSource() const { return _uploadData; }
    QVariantList downloadDataSource() const { return _downloadData; }
    QVariantList transferColumns() const;
    Q_INVOKABLE void addUploadTask(const QString& filePath, qint64 targetDirId);
    Q_INVOKABLE void removeUploadTask(int index);
    Q_INVOKABLE void removeDownloadTask(int index);

signals:
    void loginSuccess(); void loginFailed(const QString& msg); void logoutRequested();
    void downloadPathChanged(); void askDownloadLocationChanged();
    void multiThreadDownloadChanged(); void downloadSpeedLimitChanged(); void uploadSpeedLimitChanged();
    void proxyEnabledChanged(); void proxyTypeChanged(); void proxyHostChanged(); void proxyPortChanged();
    void logLevelIndexChanged();
    void fileListChanged(); void transferChanged(); void storageChanged();

private:
    void loadSettings(); void applyProxyToService(); void refreshFileList(); void updateStorageInfo();

    std::shared_ptr<app::Pan123> _pan;
    bool _isLoggedIn = false;
    int _fileCurrentDirId = 0;
    std::vector<int64_t> _dirStack;
    QVariantList _fileTableData, _fileTreeData, _breadcrumb;
    QString _storageUsed; double _storagePercent = 0;
    QVariantList _uploadData, _downloadData;
};

} // namespace app
