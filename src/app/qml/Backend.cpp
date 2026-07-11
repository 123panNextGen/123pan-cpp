#include "app/qml/Backend.h"
#include "app/common/api.hpp"
#include "app/common/config.hpp"
#include "app/common/credential.hpp"
#include "app/common/const.hpp"
#include "app/common/log.hpp"
#include <QDesktopServices>
#include <QUrl>
#include <QClipboard>
#include <QApplication>
#include <QFileInfo>

namespace app {

Backend::Backend(QObject* parent) : QObject(parent) {}
Backend::~Backend() = default;

// ========== Init & Login ==========
void Backend::init() {
    auto logger = get_logger("backend");
    logger->info("Backend 初始化");
    std::string cur = ConfigManager::get_current_account_name();
    if (!cur.empty()) {
        json acc = ConfigManager::get_account(cur);
        std::string pwd = acc.value("passWord", "");
        if (!pwd.empty()) {
            if (pwd.starts_with("enc:")) pwd = decrypt_credential(pwd);
            if (!pwd.empty()) {
                try {
                    _pan = std::make_shared<Pan123>(true, cur, pwd);
                    auto [code, _1] = _pan->get_dir(false);
                    if (code == 0) { _isLoggedIn = true; loadSettings(); fileRefresh(); emit loginSuccess(); return; }
                } catch (...) {}
            }
        }
    }
}

void Backend::login(const QString& a, const QString& p) {
    try {
        json saved = ConfigManager::get_account(a.toStdString());
        _pan = saved.empty()
            ? std::make_shared<Pan123>(false, a.toStdString(), p.toStdString())
            : std::make_shared<Pan123>(true, a.toStdString(), p.toStdString());
        int code = _pan->login();
        if (code != 200 && code != 0) { emit loginFailed(QString("登录失败，返回码: %1").arg(code)); return; }
        try { _pan->save_file(); } catch (...) {}
        _isLoggedIn = true; loadSettings(); fileRefresh(); emit loginSuccess();
    } catch (const std::exception& e) { emit loginFailed(QString::fromStdString(e.what())); }
}

void Backend::logout() {
    ConfigManager::set_current_account(""); _isLoggedIn = false; _pan.reset();
    _fileTableData.clear(); _fileTreeData.clear(); emit logoutRequested();
}
void Backend::switchAccount() { _isLoggedIn = false; _pan.reset(); emit logoutRequested(); }

QString Backend::currentUserName() const { return _pan ? QString::fromStdString(_pan->user_name) : QString(); }
QString Backend::currentDeviceInfo() const {
    if (!_pan) return {};
    return QString("%1 / %2").arg(QString::fromStdString(_pan->devicetype), QString::fromStdString(_pan->osversion));
}

// ========== Settings ==========
void Backend::loadSettings() {
    emit downloadPathChanged(); emit askDownloadLocationChanged(); emit multiThreadDownloadChanged();
    emit downloadSpeedLimitChanged(); emit uploadSpeedLimitChanged();
    emit proxyEnabledChanged(); emit proxyTypeChanged(); emit proxyHostChanged(); emit proxyPortChanged();
    emit logLevelIndexChanged();
    if (_pan) { _pan->set_download_multi_thread(multiThreadDownload()); _pan->set_download_speed_limit(downloadSpeedLimit()); _pan->set_upload_speed_limit(uploadSpeedLimit()); }
    applyProxyToService();
}

QString Backend::downloadPath() const { return QString::fromStdString(ConfigManager::get_setting<std::string>("defaultDownloadPath","")); }
void Backend::setDownloadPath(const QString& v) { ConfigManager::set_setting("defaultDownloadPath",v.toStdString()); emit downloadPathChanged(); }
bool Backend::askDownloadLocation() const { return ConfigManager::get_setting<bool>("askDownloadLocation",true); }
void Backend::setAskDownloadLocation(bool v) { ConfigManager::set_setting("askDownloadLocation",v); emit askDownloadLocationChanged(); }
bool Backend::multiThreadDownload() const { return ConfigManager::get_setting<bool>("multiThreadDownload",true); }
void Backend::setMultiThreadDownload(bool v) { ConfigManager::set_setting("multiThreadDownload",v); if(_pan)_pan->set_download_multi_thread(v); emit multiThreadDownloadChanged(); }
int Backend::downloadSpeedLimit() const { return ConfigManager::get_setting<int>("downloadSpeedLimit",0); }
void Backend::setDownloadSpeedLimit(int v) { ConfigManager::set_setting("downloadSpeedLimit",v); if(_pan)_pan->set_download_speed_limit(v); emit downloadSpeedLimitChanged(); }
int Backend::uploadSpeedLimit() const { return ConfigManager::get_setting<int>("uploadSpeedLimit",0); }
void Backend::setUploadSpeedLimit(int v) { ConfigManager::set_setting("uploadSpeedLimit",v); if(_pan)_pan->set_upload_speed_limit(v); emit uploadSpeedLimitChanged(); }
bool Backend::proxyEnabled() const { return ConfigManager::get_setting<bool>("proxyEnabled",false); }
void Backend::setProxyEnabled(bool v) { ConfigManager::set_setting("proxyEnabled",v); applyProxyToService(); emit proxyEnabledChanged(); }
QString Backend::proxyType() const { return QString::fromStdString(ConfigManager::get_setting<std::string>("proxyType","http")); }
void Backend::setProxyType(const QString& v) { ConfigManager::set_setting("proxyType",v.toStdString()); applyProxyToService(); emit proxyTypeChanged(); }
QString Backend::proxyHost() const { return QString::fromStdString(ConfigManager::get_setting<std::string>("proxyHost","")); }
void Backend::setProxyHost(const QString& v) { ConfigManager::set_setting("proxyHost",v.toStdString()); applyProxyToService(); emit proxyHostChanged(); }
int Backend::proxyPort() const { return ConfigManager::get_setting<int>("proxyPort",0); }
void Backend::setProxyPort(int v) { ConfigManager::set_setting("proxyPort",v); applyProxyToService(); emit proxyPortChanged(); }
void Backend::applyProxyToService() {
    if(!_pan)return;
    if(proxyEnabled()){ auto h=proxyHost().toStdString(); int p=proxyPort(); if(!h.empty()&&p>0){ _pan->set_download_proxy(proxyType().toStdString(),h,p); return; } }
    _pan->clear_download_proxy();
}
QStringList Backend::logLevels() const { QStringList r; for(auto& l:get_level_names()) r<<QString::fromStdString(l); return r; }
int Backend::logLevelIndex() const { auto ls=get_level_names(); auto c=get_current_level_name(); for(size_t i=0;i<ls.size();++i)if(ls[i]==c)return(int)i; return 0; }
void Backend::setLogLevelIndex(int idx) { auto ls=get_level_names(); if(idx>=0&&idx<(int)ls.size()){ set_log_level(ls[idx]); ConfigManager::set_setting("logLevel",ls[idx]); emit logLevelIndexChanged(); } }
void Backend::openLogFile() { app::open_log_file(); }
QString Backend::appVersion() const { return QString::fromUtf8(VERSION); }
void Backend::openProjectPage() { QDesktopServices::openUrl(QUrl(QString::fromUtf8(ABOUT_URL))); }
void Backend::checkUpdate() { app::check_version(); }

// ========== File columns ==========
QVariantList Backend::fileTableColumns() const {
    return { QVariantMap{{"title","名称"},{"dataIndex","name"},{"width",280}},
             QVariantMap{{"title","类型"},{"dataIndex","typeName"},{"width",120}},
             QVariantMap{{"title","大小"},{"dataIndex","size"},{"width",100}} };
}
QVariantList Backend::transferColumns() const {
    return { QVariantMap{{"title","文件名"},{"dataIndex","name"},{"width",200}},
             QVariantMap{{"title","大小"},{"dataIndex","size"},{"width",90}},
             QVariantMap{{"title","进度"},{"dataIndex","progress"},{"width",150}},
             QVariantMap{{"title","状态"},{"dataIndex","status"},{"width",80}} };
}

// ========== File operations ==========
QString Backend::storageTotal() const { return QString::fromStdString(format_file_size(MAX_STORAGE_CAPACITY)); }

void Backend::refreshFileList() {
    if(!_pan)return;
    _pan->parent_file_id=_fileCurrentDirId; _pan->file_page=0;
    auto[code,items]=_pan->get_dir_by_id(_fileCurrentDirId,false,true,100);
    if(code!=0){ emit fileListChanged(); return; }
    _fileTableData.clear(); _fileTreeData.clear();
    for(auto& item:items){
        std::string name=item.value("FileName",item.value("fileName",""));
        int type=item.value("Type",item.value("type",0));
        int64_t sz=item.value("Size",item.value("size",0));
        int64_t fid=item.value("FileId",item.value("fileId",0));
        QVariantMap row;
        row["name"]=QString::fromStdString(name);
        row["typeName"]=type==1?"📁 文件夹":QString::fromStdString(get_file_type_name(type));
        row["size"]=QString::fromStdString(format_file_size(sz));
        row["type"]=type; row["fileId"]=(qint64)fid;
        if(type==1)_fileTreeData.append(row);
        _fileTableData.append(row);
    }
    updateStorageInfo();
    emit fileListChanged();
}

void Backend::fileGoParentDir() {
    if(_dirStack.empty())return;
    _dirStack.pop_back();
    _fileCurrentDirId=_dirStack.empty()?0:_dirStack.back();
    _breadcrumb.pop_back();
    refreshFileList();
}

void Backend::fileNavigateToDir(qint64 dirId) {
    _dirStack.push_back(dirId);
    _fileCurrentDirId=dirId;
    QVariantMap b; b["title"]=QString("📁"); b["data"]=dirId; _breadcrumb.append(b);
    refreshFileList();
}

void Backend::fileNavigateByBreadcrumb(int index) {
    if(index<0||index>=(int)_breadcrumb.size())return;
    int64_t target=0;
    if(index>0)target=_breadcrumb[index-1].toMap()["data"].toLongLong();
    _dirStack.resize(index); _breadcrumb.resize(index);
    _fileCurrentDirId=target; refreshFileList();
}

void Backend::fileRefresh() { _dirStack.clear(); _breadcrumb.clear(); _fileCurrentDirId=0; refreshFileList(); }

void Backend::fileCreateFolder(const QString& name) {
    if(!_pan||name.isEmpty())return;
    try{ _pan->mkdir(name.toStdString()); refreshFileList(); }catch(...){}
}

void Backend::fileDeleteItem(qint64 fileId) {
    if(!_pan)return;
    for(size_t i=0;i<_pan->list.size();++i){
        int64_t fid=_pan->list[i].value("FileId",_pan->list[i].value("fileId",0));
        if(fid==fileId){ _pan->delete_file((int)i,true,true); refreshFileList(); return; }
    }
}

void Backend::fileRenameItem(qint64 fileId, const QString& newName) {
    if(!_pan)return;
    try{ _pan->rename_file(fileId,newName.toStdString()); refreshFileList(); }catch(...){}
}

void Backend::fileDownloadItem(qint64 fileId) {
    if(!_pan)return;
    for(auto& item:_pan->list){
        int64_t fid=item.value("FileId",item.value("fileId",0));
        if(fid!=fileId)continue;
        std::string name=item.value("FileName",item.value("fileName",""));
        int64_t sz=item.value("Size",item.value("size",0));
        QVariantMap t; t["name"]=QString::fromStdString(name);
        t["size"]=QString::fromStdString(format_file_size(sz));
        t["progress"]="0%"; t["status"]="已添加";
        _downloadData.append(t); emit transferChanged();
        return;
    }
}

void Backend::fileShareItem(qint64 fileId) {
    if(!_pan)return;
    try{
        std::string url=_pan->share({fileId},"");
        QApplication::clipboard()->setText(QString::fromStdString(url));
        get_logger("backend")->info("分享链接已复制: {}",url);
    }catch(const std::exception& e){ get_logger("backend")->error("分享失败: {}",e.what()); }
}

void Backend::fileCopyLink(qint64 fileId) {
    if(!_pan)return;
    for(auto& item:_pan->list){
        if(item.value("FileId",item.value("fileId",0))==fileId){
            std::string url=_pan->link_by_fileDetail(item,false);
            QApplication::clipboard()->setText(QString::fromStdString(url));
            return;
        }
    }
}

void Backend::fileUploadItem() {
    // Open native file dialog — placeholder for now
}

void Backend::updateStorageInfo() {
    if(!_pan)return;
    int64_t used=0;
    for(auto& item:_pan->list){ used+=item.value("Size",item.value("size",0)); }
    _storageUsed=QString::fromStdString(format_file_size(used));
    _storagePercent=MAX_STORAGE_CAPACITY>0?(double)used/MAX_STORAGE_CAPACITY*100:0;
    emit storageChanged();
}

// ========== Transfer ==========
void Backend::addUploadTask(const QString& filePath, qint64 targetDirId) {
    QVariantMap t; t["name"]=QFileInfo(filePath).fileName();
    t["size"]="..."; t["progress"]="0%"; t["status"]="等待中";
    _uploadData.append(t); emit transferChanged();
}
void Backend::removeUploadTask(int index) { if(index>=0&&index<_uploadData.size()){ _uploadData.removeAt(index); emit transferChanged(); } }
void Backend::removeDownloadTask(int index) { if(index>=0&&index<_downloadData.size()){ _downloadData.removeAt(index); emit transferChanged(); } }

} // namespace app

#include "Backend.moc"
