#include "app/service/upload_service.hpp"
#include "app/api/session.hpp"
#include "app/common/log.hpp"
#include "app/common/speed_limiter.hpp"

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <thread>

// OpenSSL MD5
#include <openssl/md5.h>

namespace app {

namespace fs = std::filesystem;

UploadService::UploadService(std::shared_ptr<NetSession> session)
    : _session(std::move(session)) {}

void UploadService::set_upload_speed_limit(int kbps) {
    if (kbps > 0) {
        _session->set_speed_limiter(std::make_shared<SpeedLimiter>(kbps), true);
    } else {
        _session->set_speed_limiter(nullptr, true);
    }
}

std::string UploadService::compute_file_md5(const std::string& file_path) {
    MD5_CTX ctx;
    MD5_Init(&ctx);

    std::ifstream f(file_path, std::ios::binary);
    if (!f) throw std::runtime_error("Cannot open file for MD5: " + file_path);

    constexpr size_t BUF_SIZE = 64 * 1024;
    std::vector<uint8_t> buf(BUF_SIZE);

    while (f) {
        f.read(reinterpret_cast<char*>(buf.data()), BUF_SIZE);
        auto bytes = f.gcount();
        if (bytes > 0) {
            MD5_Update(&ctx, buf.data(), bytes);
        }
    }

    std::vector<uint8_t> hash(MD5_DIGEST_LENGTH);
    MD5_Final(hash.data(), &ctx);

    // Convert to hex
    std::string result;
    result.reserve(MD5_DIGEST_LENGTH * 2);
    for (auto b : hash) {
        constexpr char hex[] = "0123456789abcdef";
        result.push_back(hex[b >> 4]);
        result.push_back(hex[b & 0xF]);
    }
    return result;
}

std::string UploadService::up_load(const std::string& file_path,
                                     int64_t parent_file_id, int dup_choice,
                                     std::function<void(int)> progress_callback,
                                     std::function<bool()> is_cancelled) {
    auto logger = get_logger("upload");

    // Normalize path
    std::string normalized_path = file_path;
    // Remove quotes
    normalized_path.erase(std::remove(normalized_path.begin(), normalized_path.end(), '"'),
                          normalized_path.end());
    // Backslashes to forward slashes
    std::replace(normalized_path.begin(), normalized_path.end(), '\\', '/');

    fs::path fp(normalized_path);
    std::string file_name = fp.filename().string();

    if (!fs::exists(fp)) {
        throw std::runtime_error("文件不存在: " + normalized_path);
    }
    if (fs::is_directory(fp)) {
        throw std::runtime_error("不支持文件夹上传: " + normalized_path);
    }

    int64_t fsize = fs::file_size(fp);
    logger->info("上传开始: {} ({:.2f} MB)", file_name,
                static_cast<double>(fsize) / 1024.0 / 1024.0);

    auto t0 = std::chrono::steady_clock::now();

    std::string readable_hash = compute_file_md5(normalized_path);
    logger->debug("文件 MD5 计算完成: {}", readable_hash);

    if (is_cancelled && is_cancelled()) {
        return "已取消";
    }

    json list_up_request = {
        {"driveId", 0},
        {"etag", readable_hash},
        {"fileName", file_name},
        {"parentFileId", parent_file_id},
        {"size", fsize},
        {"type", 0},
        {"duplicate", 0},
    };

    auto& http = _session->http_session();
    http.SetUrl(cpr::Url{"https://www.123pan.cn/b/api/file/upload_request"});
    http.SetBody(cpr::Body{list_up_request.dump()});
    http.SetTimeout(cpr::Timeout{30000});
    cpr::Response up_res = http.Post();

    auto up_res_json = json::parse(up_res.text);
    int res_code_up = up_res_json.value("code", -1);

    // Handle duplicate file (code 5060)
    if (res_code_up == 5060) {
        list_up_request["duplicate"] = dup_choice;
        http.SetBody(cpr::Body{list_up_request.dump()});
        up_res = http.Post();
        up_res_json = json::parse(up_res.text);
        res_code_up = up_res_json.value("code", -1);
    }

    if (res_code_up != 0) {
        throw std::runtime_error("上传请求失败: " + up_res_json.dump());
    }

    // Check if file was reused (秒传)
    if (up_res_json["data"].value("Reuse", false)) {
        int64_t up_file_id = up_res_json["data"]["FileId"].get<int64_t>();
        auto elapsed = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - t0).count();
        double speed = elapsed > 0 ? fsize / 1024.0 / 1024.0 / elapsed : 0;
        logger->info("上传完成(复用): {} ({:.2f} MB / {:.1f}s / {:.1f} MB/s)",
                    file_name, fsize / 1024.0 / 1024.0, elapsed, speed);
        return std::to_string(up_file_id);
    }

    // Get upload parameters
    std::string bucket = up_res_json["data"]["Bucket"].get<std::string>();
    std::string storage_node = up_res_json["data"]["StorageNode"].get<std::string>();
    std::string upload_key = up_res_json["data"]["Key"].get<std::string>();
    std::string upload_id = up_res_json["data"]["UploadId"].get<std::string>();
    int64_t up_file_id = up_res_json["data"]["FileId"].get<int64_t>();

    // Start multipart upload
    json start_data = {
        {"bucket", bucket},
        {"key", upload_key},
        {"uploadId", upload_id},
        {"storageNode", storage_node},
    };

    http.SetUrl(cpr::Url{"https://www.123pan.cn/b/api/file/s3_list_upload_parts"});
    http.SetBody(cpr::Body{start_data.dump()});
    http.SetTimeout(cpr::Timeout{30000});
    cpr::Response start_res = http.Post();

    auto start_res_json = json::parse(start_res.text);
    res_code_up = start_res_json.value("code", -1);
    if (res_code_up != 0) {
        throw std::runtime_error("获取传输列表失败: " + start_res_json.dump());
    }

    // Upload parts
    constexpr int64_t block_size = 5 * 1024 * 1024;  // 5MB parts
    int64_t total_sent = 0;
    int part_number = 1;

    std::ifstream file_stream(normalized_path, std::ios::binary);
    if (!file_stream) {
        throw std::runtime_error("无法打开文件: " + normalized_path);
    }

    std::vector<uint8_t> buffer(block_size);

    while (file_stream) {
        if (is_cancelled && is_cancelled()) {
            return "已取消";
        }

        file_stream.read(reinterpret_cast<char*>(buffer.data()), block_size);
        auto bytes_read = file_stream.gcount();
        if (bytes_read == 0) break;

        // Get upload URL for this part
        json get_link_data = {
            {"bucket", bucket},
            {"key", upload_key},
            {"partNumberEnd", part_number + 1},
            {"partNumberStart", part_number},
            {"uploadId", upload_id},
            {"StorageNode", storage_node},
        };

        http.SetUrl(cpr::Url{"https://www.123pan.cn/b/api/file/s3_repare_upload_parts_batch"});
        http.SetBody(cpr::Body{get_link_data.dump()});
        http.SetTimeout(cpr::Timeout{30000});
        cpr::Response get_link_res = http.Post();

        auto get_link_res_json = json::parse(get_link_res.text);
        res_code_up = get_link_res_json.value("code", -1);
        if (res_code_up != 0) {
            throw std::runtime_error("获取链接失败: " + get_link_res_json.dump());
        }

        std::string upload_url = get_link_res_json["data"]["presignedUrls"]
                                  [std::to_string(part_number)].get<std::string>();

        // Upload the part
        cpr::Response put_res = cpr::Put(
            cpr::Url{upload_url},
            cpr::Body{std::string(reinterpret_cast<char*>(buffer.data()),
                                   static_cast<size_t>(bytes_read))},
            cpr::Timeout{cpr::Timeout{60000}}
        );

        total_sent += bytes_read;

        if (progress_callback && fsize > 0) {
            int pct = static_cast<int>(total_sent * 100 / fsize);
            progress_callback(pct);
        }

        ++part_number;
    }

    // Complete multipart upload
    json uploaded_comp_data = {
        {"bucket", bucket},
        {"key", upload_key},
        {"uploadId", upload_id},
        {"storageNode", storage_node},
    };

    http.SetUrl(cpr::Url{"https://www.123pan.cn/b/api/file/s3_list_upload_parts"});
    http.SetBody(cpr::Body{uploaded_comp_data.dump()});
    http.SetTimeout(cpr::Timeout{30000});
    http.Post();

    http.SetUrl(cpr::Url{"https://www.123pan.cn/b/api/file/s3_complete_multipart_upload"});
    http.SetBody(cpr::Body{uploaded_comp_data.dump()});
    http.SetTimeout(cpr::Timeout{30000});
    http.Post();

    // Wait for large files
    if (fsize > 64 * 1024 * 1024) {
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }

    // Close upload session
    json close_data = {{"fileId", up_file_id}};
    http.SetUrl(cpr::Url{"https://www.123pan.cn/b/api/file/upload_complete"});
    http.SetBody(cpr::Body{close_data.dump()});
    http.SetTimeout(cpr::Timeout{30000});
    cpr::Response close_res = http.Post();

    auto close_res_json = json::parse(close_res.text);
    res_code_up = close_res_json.value("code", -1);
    if (res_code_up != 0) {
        throw std::runtime_error("上传完成确认失败: " + close_res_json.dump());
    }

    auto elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - t0).count();
    double speed = elapsed > 0 ? fsize / 1024.0 / 1024.0 / elapsed : 0;
    logger->info("上传完成: {} ({:.2f} MB / {:.1f}s / {:.1f} MB/s)",
                file_name, fsize / 1024.0 / 1024.0, elapsed, speed);

    return std::to_string(up_file_id);
}

}  // namespace app
