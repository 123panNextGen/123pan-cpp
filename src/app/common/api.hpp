#pragma once

#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <filesystem>

namespace app {

class NetSession;
class AuthService;
class FileService;
class DownloadService;
class UploadService;
class SpeedLimiter;

/// 123云盘API客户端类（向后兼容包装层）。
/// 内部使用服务类处理具体业务逻辑。
class Pan123 {
public:
    Pan123(bool readfile = true,
           const std::string& user_name = "",
           const std::string& password = "",
           const std::string& authorization = "");

    /// 登录。
    int login();

    /// 保存账户信息到配置文件。
    void save_file();

    /// 获取当前目录文件列表（使用 parent_file_id）。
    std::pair<int, std::vector<nlohmann::json>> get_dir(bool save = true);

    /// 按目录ID获取文件列表。
    std::pair<int, std::vector<nlohmann::json>>
    get_dir_by_id(int64_t file_id, bool save = true,
                  bool all = false, int limit = 100);

    /// 按文件详情获取下载链接。
    std::string link_by_fileDetail(const nlohmann::json& file_detail,
                                   bool show_link = true);

    /// 删除文件（按索引）。
    void delete_file(int file_index, bool by_num = true, bool operation = true);

    /// 重命名文件。
    bool rename_file(int64_t file_id, const std::string& new_name);

    /// 创建分享链接。
    std::string share(const std::vector<int64_t>& file_id_list,
                     const std::string& share_pwd = "");

    /// 上传文件。
    std::string up_load(const std::string& file_path,
                       std::function<void(int)> progress_callback = nullptr,
                       std::function<bool()> is_cancelled = nullptr);

    /// 创建文件夹。
    /// @return 文件ID，失败返回nullopt
    std::optional<int64_t> mkdir(const std::string& dirname, bool remakedir = false);

    // ---- Session 配置门面方法 ----
    void set_download_multi_thread(bool enabled);
    void set_download_speed_limit(int kbps);
    void set_upload_speed_limit(int kbps);
    void set_download_proxy(const std::string& proxy_type, const std::string& host,
                           int port, const std::string& username = "",
                           const std::string& password = "");
    void clear_download_proxy();

    bool download_file(const std::string& url,
                      const std::filesystem::path& file_path,
                      int64_t file_size,
                      std::function<void(int64_t, int64_t)> progress_callback = nullptr);

    // 公共属性
    std::string devicetype;
    std::string osversion;
    std::string loginuuid;
    std::string user_name;
    std::string password;
    std::string authorization;

    // 目录浏览状态
    std::vector<nlohmann::json> list;
    int total = 0;
    bool all_file = false;
    int file_page = 0;
    int64_t parent_file_id = 0;

private:
    void sync_to_session();
    void sync_from_auth();

    std::shared_ptr<NetSession> _session;
    std::unique_ptr<AuthService> _auth;
    std::unique_ptr<FileService> _file;
    std::unique_ptr<DownloadService> _download;
    std::unique_ptr<UploadService> _upload;
};

/// 格式化文件大小（工具函数）。
std::string format_file_size(int64_t size);

/// 获取文件扩展名。
std::string get_file_extension(const std::string& filename);

/// 获取文件类型名称。
std::string get_file_type_name(int file_type);

/// 检查是否为重复文件名。
bool is_duplicate_filename(const Pan123& pan, const std::string& filename);

/// 检查版本更新。
bool check_version();

}  // namespace app
