#include "app/common/api.hpp"
#include "app/api/session.hpp"
#include "app/service/auth_service.hpp"
#include "app/service/file_service.hpp"
#include "app/service/download_service.hpp"
#include "app/service/upload_service.hpp"
#include "app/common/config.hpp"
#include "app/common/log.hpp"
#include "app/common/credential.hpp"
#include "app/data/devices.hpp"

#include <random>

namespace app {

// ============================================================
// Pan123
// ============================================================
Pan123::Pan123(bool readfile, const std::string& user_name,
               const std::string& password, const std::string& authorization) {
    auto logger = get_logger("pan123");

    _session = std::make_shared<NetSession>();
    _auth = std::make_unique<AuthService>(_session);
    _file = std::make_unique<FileService>(_session);
    _download = std::make_unique<DownloadService>(_session);
    _upload = std::make_unique<UploadService>(_session);

    // Copy device info from auth
    devicetype = _auth->devicetype;
    osversion = _auth->osversion;
    loginuuid = _auth->loginuuid;

    if (readfile) {
        _auth->read_ini(user_name, password, authorization);
        sync_from_auth();
    } else {
        if (user_name.empty() || password.empty()) {
            throw std::runtime_error("用户名或密码为空");
        }
        _auth->user_name = user_name;
        _auth->password = password;
        _auth->authorization = authorization;
        sync_from_auth();
    }

    sync_to_session();

    // Verify with get_dir
    auto [res_code, items] = get_dir();
    if (res_code != 0) {
        login();
        get_dir();
    }
}

Pan123::~Pan123() = default;

void Pan123::sync_to_session() {
    _auth->devicetype = devicetype;
    _auth->osversion = osversion;
    _auth->loginuuid = loginuuid;
    _auth->user_name = user_name;
    _auth->password = password;
    _auth->authorization = authorization;
    _auth->sync_to_session();
}

void Pan123::sync_from_auth() {
    devicetype = _auth->devicetype;
    osversion = _auth->osversion;
    loginuuid = _auth->loginuuid;
    user_name = _auth->user_name;
    password = _auth->password;
    authorization = _auth->authorization;
}

int Pan123::login() {
    auto logger = get_logger("pan123");
    logger->info("Pan123.login: user={}", user_name);
    _auth->user_name = user_name;
    _auth->password = password;
    int code = _auth->login();
    if (code == 200) {
        authorization = _auth->authorization;
        save_file();
    }
    return code;
}

void Pan123::save_file() {
    _auth->devicetype = devicetype;
    _auth->osversion = osversion;
    _auth->loginuuid = loginuuid;
    _auth->user_name = user_name;
    _auth->password = password;
    _auth->authorization = authorization;
    _auth->save_file();
}

std::pair<int, std::vector<json>> Pan123::get_dir(bool save) {
    return get_dir_by_id(parent_file_id, save);
}

std::pair<int, std::vector<json>>
Pan123::get_dir_by_id(int64_t file_id, bool save, bool all, int limit) {
    auto logger = get_logger("pan123");

    auto [code, items, total, all_file, pages_read] =
        _file->get_dir_by_id(file_id, file_page,
                              static_cast<int>(list.size()), all, limit);

    if (code == 2) {
        logger->warn("token 过期，正在尝试重新登录");
        int login_code = login();
        if (login_code == 200) {
            return get_dir_by_id(file_id, save, all, limit);
        }
        logger->error("重新登录失败");
        return {code, {}};
    }

    if (code != 0) {
        logger->error("获取文件列表失败: file_id={}, code={}", file_id, code);
        return {code, {}};
    }

    this->total = total;
    this->all_file = all_file;
    ++file_page;

    if (save) {
        list.insert(list.end(), items.begin(), items.end());
    }

    return {0, items};
}

std::string Pan123::link_by_fileDetail(const json& file_detail, bool show_link) {
    return _download->link_by_fileDetail(file_detail, show_link);
}

void Pan123::delete_file(int file_index, bool by_num, bool operation) {
    _file->delete_file(list, file_index, by_num, operation);
}

bool Pan123::rename_file(int64_t file_id, const std::string& new_name) {
    return _file->rename_file(file_id, new_name);
}

std::string Pan123::share(const std::vector<int64_t>& file_id_list,
                          const std::string& share_pwd) {
    return _file->share(file_id_list, share_pwd);
}

std::string Pan123::up_load(const std::string& file_path,
                            std::function<void(int)> progress_callback,
                            std::function<bool()> is_cancelled) {
    return _upload->up_load(file_path, parent_file_id, 0,
                            std::move(progress_callback),
                            std::move(is_cancelled));
}

std::optional<int64_t> Pan123::mkdir(const std::string& dirname, bool remakedir) {
    auto [file_id, err] = _file->mkdir(dirname, list, parent_file_id, remakedir);
    if (!err.empty()) return std::nullopt;

    // Refresh directory
    get_dir();
    return file_id;
}

// ---- Session 配置门面方法 ----
void Pan123::set_download_multi_thread(bool enabled) {
    _download->set_multi_thread(enabled);
}

void Pan123::set_download_speed_limit(int kbps) {
    _download->set_download_speed_limit(kbps);
}

void Pan123::set_upload_speed_limit(int kbps) {
    _upload->set_upload_speed_limit(kbps);
}

void Pan123::set_download_proxy(const std::string& proxy_type,
                                 const std::string& host, int port,
                                 const std::string& username,
                                 const std::string& password) {
    _download->set_proxy(proxy_type, host, port, username, password);
}

void Pan123::clear_download_proxy() {
    _download->clear_proxy();
}

bool Pan123::download_file(
    const std::string& url,
    const std::filesystem::path& file_path,
    int64_t file_size,
    std::function<void(int64_t, int64_t)> progress_callback
) {
    return _download->download_file(url, file_path, file_size,
                                     std::move(progress_callback));
}

// ============================================================
// Utility functions
// ============================================================
std::string get_file_extension(const std::string& filename) {
    auto pos = filename.rfind('.');
    if (pos == std::string::npos || pos == 0) return "";
    return filename.substr(pos);
}

std::string get_file_type_name(int file_type) {
    return (file_type == 1) ? "文件夹" : "文件";
}

bool is_duplicate_filename(const Pan123& pan, const std::string& filename) {
    for (auto& item : pan.list) {
        std::string name = item.value("FileName", item.value("fileName", ""));
        if (name == filename) return true;
    }
    return false;
}

}  // namespace app
