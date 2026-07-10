#pragma once

#include <nlohmann/json.hpp>

#include <memory>
#include <string>
#include <functional>
#include <filesystem>

namespace app {

class NetSession;
class SpeedLimiter;

/// 下载服务。
class DownloadService {
public:
    explicit DownloadService(std::shared_ptr<NetSession> session);

    /// 按文件详情获取下载链接。
    /// @return 下载URL（成功）或错误码（负整数表示失败）
    std::string link_by_fileDetail(const nlohmann::json& file_detail,
                                   bool show_link = true);

    /// 从URL下载文件（简单单线程）。
    void download_from_url(const std::string& url, const std::string& file_name,
                          const std::string& download_path = "download");

    /// 启用或禁用多线程下载。
    void set_multi_thread(bool enabled);

    /// 设置下载速度限制（KB/s），0 为不限速。
    void set_download_speed_limit(int kbps);

    /// 设置下载代理。
    void set_proxy(const std::string& proxy_type, const std::string& host,
                   int port, const std::string& username = "",
                   const std::string& password = "");

    /// 清除下载代理。
    void clear_proxy();

    /// 多线程分片下载文件。
    bool download_file(const std::string& url,
                      const std::filesystem::path& file_path,
                      int64_t file_size,
                      std::function<void(int64_t, int64_t)> progress_callback = nullptr);

private:
    std::shared_ptr<NetSession> _session;
};

}  // namespace app
