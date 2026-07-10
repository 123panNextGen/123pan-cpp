#pragma once

#include "app/api/model.hpp"

#include <cpr/cpr.h>
#include <memory>
#include <string>
#include <functional>
#include <optional>
#include <mutex>
#include <filesystem>

namespace app {

class SpeedLimiter;

/// 123云盘 HTTP API 会话层，负责所有 HTTP 请求。
/// 对应 Python NetSession。
class NetSession {
public:
    NetSession();
    ~NetSession();

    NetSession(const NetSession&) = delete;
    NetSession& operator=(const NetSession&) = delete;
    NetSession(NetSession&&) = delete;
    NetSession& operator=(NetSession&&) = delete;

    // ---- 用户信息 ----
    void set_user_info(const UserInfoModel& user_info);
    const UserInfoModel* user_info() const { return _user_info.get(); }
    std::string authorization() const;

    // ---- 多线程下载配置 ----
    void set_multi_thread(bool enabled, int num_threads = 4);
    void set_speed_limiter(std::shared_ptr<SpeedLimiter> limiter, bool is_upload = false);
    using ProgressCallback = std::function<void(int64_t downloaded, int64_t total)>;
    void set_progress_callback(ProgressCallback callback);

    // ---- 代理配置 ----
    void set_proxy(const std::string& proxy_url);
    void set_proxy_auth(const std::string& proxy_type, const std::string& host,
                        int port, const std::string& username = "",
                        const std::string& password = "");

    // ---- 账户 API ----
    ApiReturnModel login(const std::string& user_name, const std::string& password);

    // ---- 文件列表 API ----
    ApiReturnModel get_file_list(int64_t file_id = 0, bool reverse = false,
                                 bool trashed = false, int page = 1,
                                 int limit = 100, bool retry_login = true);
    ApiReturnModel get_trash_list(int64_t file_id = 0);

    // ---- 文件夹操作 ----
    ApiReturnModel create_dir(const std::string& dir_name, int64_t parent_file_id);

    // ---- 删除/恢复 ----
    ApiReturnModel trash_file(const json& file_info, bool operation = true);
    ApiReturnModel restore_file(const json& file_info);

    // ---- 下载链接 ----
    ApiReturnModel get_file_link(const json& file_info);

    // ---- 重命名 ----
    ApiReturnModel rename_file(int64_t file_id, const std::string& new_name);

    // ---- 多线程下载 ----
    bool download_file_multithread(
        const std::string& url,
        const std::filesystem::path& file_path,
        int64_t file_size,
        ProgressCallback progress_callback = nullptr
    );

    // ---- 公共属性 ----
    cpr::Session& http_session() { return _http; }

    // JSON 安全解析（公开以支持测试）
    static std::pair<json, std::optional<ApiReturnModel>> safe_json(
        const cpr::Response& resp
    );

private:
    cpr::Header build_headers() const;
    void update_headers();

    // 单线程下载
    bool download_single(
        const std::string& url,
        const std::filesystem::path& file_path,
        int64_t file_size,
        ProgressCallback progress_callback = nullptr,
        int redirect_count = 0
    );

    // 多线程分片下载
    bool download_chunked(
        const std::string& url,
        const std::filesystem::path& file_path,
        int64_t file_size,
        ProgressCallback progress_callback = nullptr
    );

    // 检查 Range 支持
    bool check_range_support(const std::string& url);

    // 探测 JSON 重定向
    std::string resolve_json_redirect_url(const std::string& url);

    // 检查 JSON 重定向响应
    static std::string check_json_redirect(const cpr::Response& resp);

    // URL 重写（绕过流量限制）
    static std::string rewrite_download_url(const std::string& url);

    // 解析重定向获取真实下载链接
    std::string resolve_download_url(const std::string& url);

    // 从 download-v2 URL 解码
    static std::string decode_download_v2_params(const std::string& url);

    // Base64 编解码
    static std::string b64_encode(const std::string& data);
    static std::string b64_decode(const std::string& data);

    // 成员变量
    std::unique_ptr<UserInfoModel> _user_info;
    cpr::Session _http;
    cpr::Session _transfer;

    bool _multi_thread_enabled = true;
    int _num_threads = 4;
    int64_t _chunk_size = 1024 * 1024;  // 1MB

    std::shared_ptr<SpeedLimiter> _download_limiter;
    std::shared_ptr<SpeedLimiter> _upload_limiter;
    ProgressCallback _progress_callback;

    // 下载流量限制错误码
    static constexpr auto DOWNLOAD_LIMIT_CODES = {5113, 5114};

    mutable std::mutex _mutex;

    // 代理状态
    std::string _proxy_url;
};

/// 格式化文件大小
std::string format_file_size(int64_t size);

/// 检查版本更新
bool check_version();

}  // namespace app
