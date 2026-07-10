#include "app/service/download_service.hpp"
#include "app/api/session.hpp"
#include "app/common/log.hpp"
#include "app/common/speed_limiter.hpp"

#include <filesystem>
#include <fstream>

namespace app {

namespace fs = std::filesystem;

DownloadService::DownloadService(std::shared_ptr<NetSession> session)
    : _session(std::move(session)) {}

std::string DownloadService::link_by_fileDetail(const nlohmann::json& file_detail,
                                                  bool show_link) {
    auto logger = get_logger("download");
    auto result = _session->get_file_link(file_detail);

    if (result.code != 0) {
        logger->error("获取下载链接失败，返回码: {}", result.code);
        logger->error("{}", result.msg);
        return std::to_string(result.code);
    }

    std::string redirect_url = result.data.get<std::string>();
    if (show_link) {
        logger->info("获取下载链接成功: {}", redirect_url);
    }
    return redirect_url;
}

void DownloadService::download_from_url(const std::string& url,
                                          const std::string& file_name,
                                          const std::string& download_path) {
    auto logger = get_logger("download");
    fs::path download_dir(download_path);
    if (!fs::exists(download_dir)) {
        logger->info("创建下载目录");
        fs::create_directories(download_dir);
    }

    fs::path file_path = download_dir / file_name;
    fs::path temp_path = file_path;
    temp_path += ".123pan";

    if (fs::exists(temp_path)) {
        fs::remove(temp_path);
    }

    int redirect_count = 0;
    std::string current_url = url;

    while (redirect_count < 3) {
        cpr::Response resp = _session->http_session().Get(
            cpr::Url{current_url},
            cpr::Timeout{10000}
        );

        std::string content_type;
        if (resp.header.find("Content-Type") != resp.header.end()) {
            content_type = resp.header.at("Content-Type");
        }

        if (content_type.find("json") == std::string::npos) {
            // Save file
            std::ofstream out(temp_path, std::ios::binary);
            if (!out) throw std::runtime_error("Cannot create temp file");
            out.write(resp.text.data(), resp.text.size());
            out.close();
            break;
        }

        try {
            json body = json::parse(resp.text);
            std::string redirect_url = body.value("data", json{}).value("RedirectUrl",
                                        body.value("data", json{}).value("redirect_url", ""));

            if (!redirect_url.empty() && redirect_url.starts_with("http")) {
                logger->info("下载遇到 JSON 重定向: {} ...", redirect_url.substr(0, 80));
                current_url = redirect_url;
                ++redirect_count;
                continue;
            }

            std::string msg = body.value("message", body.value("msg", "未知错误"));
            logger->error("CDN 返回 JSON 错误: {}", body.dump());
            throw std::runtime_error("下载 " + file_name + " 失败，CDN 返回: " + msg);
        } catch (const json::parse_error&) {
            // Not JSON, just save
            std::ofstream out(temp_path, std::ios::binary);
            out.write(resp.text.data(), resp.text.size());
            out.close();
            break;
        }
    }

    if (fs::exists(file_path)) fs::remove(file_path);
    fs::rename(temp_path, file_path);
}

void DownloadService::set_multi_thread(bool enabled) {
    _session->set_multi_thread(enabled);
}

void DownloadService::set_download_speed_limit(int kbps) {
    if (kbps > 0) {
        _session->set_speed_limiter(std::make_shared<SpeedLimiter>(kbps), false);
    } else {
        _session->set_speed_limiter(nullptr, false);
    }
}

void DownloadService::set_proxy(const std::string& proxy_type, const std::string& host,
                                 int port, const std::string& username,
                                 const std::string& password) {
    _session->set_proxy_auth(proxy_type, host, port, username, password);
}

void DownloadService::clear_proxy() {
    _session->set_proxy("");
}

bool DownloadService::download_file(
    const std::string& url,
    const fs::path& file_path,
    int64_t file_size,
    std::function<void(int64_t, int64_t)> progress_callback
) {
    return _session->download_file_multithread(url, file_path, file_size,
                                                std::move(progress_callback));
}

}  // namespace app
