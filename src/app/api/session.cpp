#include "app/api/session.hpp"
#include "app/common/log.hpp"
#include "app/common/speed_limiter.hpp"

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

#include <format>
#include <regex>
#include <thread>
#include <future>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <queue>

using namespace std::chrono_literals;

namespace app {

namespace fs = std::filesystem;

// ============================================================
// Base64 helpers (URL-safe)
// ============================================================
static const char B64_CHARS[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char B64_URL_CHARS[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

std::string NetSession::b64_encode(const std::string& data) {
    std::string result;
    result.reserve(((data.size() + 2) / 3) * 4);
    auto bytes = reinterpret_cast<const uint8_t*>(data.data());
    size_t len = data.size();

    for (size_t i = 0; i < len; i += 3) {
        uint32_t triple = (static_cast<uint32_t>(bytes[i]) << 16);
        if (i + 1 < len) triple |= (static_cast<uint32_t>(bytes[i + 1]) << 8);
        if (i + 2 < len) triple |= static_cast<uint32_t>(bytes[i + 2]);

        result.push_back(B64_URL_CHARS[(triple >> 18) & 0x3F]);
        result.push_back(B64_URL_CHARS[(triple >> 12) & 0x3F]);
        result.push_back((i + 1 < len) ? B64_URL_CHARS[(triple >> 6) & 0x3F] : '=');
        result.push_back((i + 2 < len) ? B64_URL_CHARS[triple & 0x3F] : '=');
    }
    return result;
}

std::string NetSession::b64_decode(const std::string& data) {
    if (data.empty()) return "";

    // Try standard base64 first, then URL-safe
    auto decode_impl = [](const std::string& input, bool url_safe) -> std::string {
        std::string result;
        result.reserve(input.size() * 3 / 4);
        auto char_val = [url_safe](char c) -> int {
            if (c >= 'A' && c <= 'Z') return c - 'A';
            if (c >= 'a' && c <= 'z') return c - 'a' + 26;
            if (c >= '0' && c <= '9') return c - '0' + 52;
            if (url_safe) {
                if (c == '-') return 62;
                if (c == '_') return 63;
            } else {
                if (c == '+') return 62;
                if (c == '/') return 63;
            }
            return -1;
        };

        int val = 0, valb = -8;
        for (char c : input) {
            if (c == '=') break;
            int cv = char_val(c);
            if (cv == -1) continue;
            val = (val << 6) + cv;
            valb += 6;
            if (valb >= 0) {
                result.push_back(static_cast<char>((val >> valb) & 0xFF));
                valb -= 8;
            }
        }
        return result;
    };

    auto decoded = decode_impl(data, false);
    if (!decoded.empty()) return decoded;
    return decode_impl(data, true);
}

// ============================================================
// NetSession Constructor / Destructor
// ============================================================
NetSession::NetSession() {
    // Configure HTTP session headers
    _http.SetHeader(cpr::Header{
        {"accept-encoding", "gzip"},
        {"content-type", "application/json"},
        {"platform", "android"},
        {"devicename", "Xiaomi"},
        {"host", "www.123pan.cn"},
        {"app-version", "61"},
        {"x-app-version", "2.4.0"},
    });

    // Configure transfer session (for CDN downloads/uploads)
    _transfer.SetTimeout(cpr::Timeout{30000});  // 30s timeout
    // CRL: use longer timeout for transfers
    _transfer.SetConnectTimeout(cpr::ConnectTimeout{10000});

    _http.SetTimeout(cpr::Timeout{30000});
}

NetSession::~NetSession() = default;

// ============================================================
// User info & headers
// ============================================================
void NetSession::set_user_info(const UserInfoModel& user_info) {
    std::lock_guard<std::mutex> lock(_mutex);
    _user_info = std::make_unique<UserInfoModel>(user_info);
    update_headers();
}

std::string NetSession::authorization() const {
    std::lock_guard<std::mutex> lock(_mutex);
    if (_user_info) return _user_info->authorization;
    return "";
}

cpr::Header NetSession::build_headers() const {
    cpr::Header headers;
    if (!_user_info) return headers;

    auto& device = _user_info->device;
    if (!device.os.empty()) {
        headers["user-agent"] = std::format("123pan/v2.4.0({};Xiaomi)", device.os);
        headers["osversion"] = device.os;
        headers["devicetype"] = device.type;
    }
    headers["loginuuid"] = _user_info->uuid;
    if (!_user_info->authorization.empty()) {
        headers["authorization"] = _user_info->authorization;
    }
    return headers;
}

void NetSession::update_headers() {
    auto h = build_headers();
    for (auto& [key, val] : h) {
        _http.UpdateHeader(cpr::Header{{key, val}});
    }
}

// ============================================================
// Multi-thread / speed / proxy config
// ============================================================
void NetSession::set_multi_thread(bool enabled, int num_threads) {
    _multi_thread_enabled = enabled;
    _num_threads = std::clamp(num_threads, 1, 16);
}

void NetSession::set_speed_limiter(std::shared_ptr<SpeedLimiter> limiter, bool is_upload) {
    if (is_upload) {
        _upload_limiter = std::move(limiter);
    } else {
        _download_limiter = std::move(limiter);
    }
}

void NetSession::set_progress_callback(ProgressCallback callback) {
    _progress_callback = std::move(callback);
}

void NetSession::set_proxy(const std::string& proxy_url) {
    _proxy_url = proxy_url;
    if (proxy_url.empty()) {
        _http.SetProxies(cpr::Proxies{});
        _transfer.SetProxies(cpr::Proxies{});
    } else {
        cpr::Proxies proxies{{"http", proxy_url}, {"https", proxy_url}};
        _http.SetProxies(proxies);
        _transfer.SetProxies(proxies);
    }
}

void NetSession::set_proxy_auth(const std::string& proxy_type, const std::string& host,
                                int port, const std::string& username,
                                const std::string& password) {
    if (host.empty() || port <= 0) {
        set_proxy("");
        return;
    }
    std::string auth = (!username.empty() && !password.empty())
        ? std::format("{}:{}@", username, password) : "";
    std::string proxy_url = std::format("{}://{}{}:{}", proxy_type, auth, host, port);
    set_proxy(proxy_url);
}

// ============================================================
// Safe JSON parsing
// ============================================================
std::pair<json, std::optional<ApiReturnModel>> NetSession::safe_json(
    const cpr::Response& resp
) {
    try {
        json body = json::parse(resp.text);
        return {body, std::nullopt};
    } catch (const json::parse_error&) {
        return {
            json{},
            ApiReturnModel(
                -1, -1, ApiCode::FAIL,
                std::format("服务器返回无效 JSON (HTTP {})", resp.status_code)
            )
        };
    }
}

// ============================================================
// Login
// ============================================================
ApiReturnModel NetSession::login(const std::string& user_name, const std::string& password) {
    auto logger = get_logger("session");
    auto t0 = std::chrono::steady_clock::now();

    cpr::Response resp;
    try {
        resp = _http.Post(
            cpr::Url{"https://www.123pan.cn/b/api/user/sign_in"},
            cpr::Body{json{{"type", 1}, {"passport", user_name}, {"password", password}}.dump()},
            cpr::Timeout{5000}
        );
    } catch (const std::exception& e) {
        auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        logger->error("登录请求失败 ({:.2f}s): {}", elapsed, e.what());
        return ApiReturnModel(-1, -1, ApiCode::FAIL, e.what());
    }

    auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    auto [body, error] = safe_json(resp);
    if (error) {
        logger->error("登录响应解析失败 ({:.2f}s): HTTP {}", elapsed, resp.status_code);
        return *error;
    }

    int code = body.value("code", -1);
    logger->info("登录 {} ({:.2f}s): HTTP {}, code={}", user_name, elapsed, resp.status_code, code);

    if (code != 200) {
        std::string msg = body.value("message", "");
        logger->error("登录失败: code={}, msg={}", code, msg);
        return ApiReturnModel(code, code, ApiCode::FAIL, msg);
    }

    std::string token = body["data"]["token"].get<std::string>();
    std::string authorization = "Bearer " + token;

    // Extract cookies from Set-Cookie header
    std::string set_cookie;
    if (resp.header.find("Set-Cookie") != resp.header.end()) {
        set_cookie = resp.header.at("Set-Cookie");
    }

    // Build user info
    _user_info = std::make_unique<UserInfoModel>();
    _user_info->user_name = user_name;
    _user_info->password = password;
    _user_info->authorization = authorization;
    update_headers();

    json data = {
        {"token", token},
        {"authorization", authorization},
        {"cookies", set_cookie},
    };

    return ApiReturnModel(200, 200, ApiCode::SUCCESS, "", data);
}

// ============================================================
// Get file list
// ============================================================
ApiReturnModel NetSession::get_file_list(int64_t file_id, bool reverse, bool trashed,
                                          int page, int limit, bool retry_login) {
    auto logger = get_logger("session");
    auto t0 = std::chrono::steady_clock::now();

    cpr::Parameters params{
        {"driveId", "0"},
        {"limit", std::to_string(limit)},
        {"next", "0"},
        {"orderBy", "file_id"},
        {"orderDirection", reverse ? "asc" : "desc"},
        {"parentFileId", std::to_string(file_id)},
        {"trashed", trashed ? "true" : "false"},
        {"SearchData", ""},
        {"Page", std::to_string(page)},
        {"OnlyLookAbnormalFile", "0"},
    };

    cpr::Response resp;
    try {
        resp = _http.Get(
            cpr::Url{"https://www.123pan.cn/api/file/list/new"},
            params,
            cpr::Timeout{30000}
        );
    } catch (const std::exception& e) {
        auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        logger->error("获取文件列表失败 ({:.2f}s): file_id={}, page={}, err={}",
                     elapsed, file_id, page, e.what());
        return ApiReturnModel(-1, -1, ApiCode::FAIL, e.what());
    }

    auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    auto [body, error] = safe_json(resp);
    if (error) {
        logger->error("文件列表响应异常: file_id={}, HTTP {}", file_id, resp.status_code);
        return *error;
    }

    int code = body.value("code", -1);
    int total = 0;
    if (body.contains("data") && body["data"].is_object()) {
        total = body["data"].value("Total", 0);
    }

    logger->debug("获取文件列表 ({:.2f}s): file_id={}, page={}, code={}, total={}",
                 elapsed, file_id, page, code, total);

    if (code == 2 && retry_login) {
        logger->warn("token 过期，需重新登录: file_id={}", file_id);
        return ApiReturnModel(code, code, ApiCode::FAIL, body.value("message", "token 过期"));
    }
    if (code != 0) {
        logger->error("获取文件列表失败: file_id={}, code={}, msg={}",
                      file_id, code, body.value("message", ""));
        return ApiReturnModel(code, code, ApiCode::FAIL, body.value("message", ""));
    }

    try {
        FileListResponse flr = FileListResponse::from_json(body);
        return ApiReturnModel(0, 200, ApiCode::SUCCESS, "", json(flr));
    } catch (const std::exception& e) {
        logger->error("解析文件列表失败: {}", e.what());
        return ApiReturnModel(-1, -1, ApiCode::FAIL, std::format("解析响应失败: {}", e.what()));
    }
}

ApiReturnModel NetSession::get_trash_list(int64_t file_id) {
    return get_file_list(file_id, false, true);
}

// ============================================================
// Create directory
// ============================================================
ApiReturnModel NetSession::create_dir(const std::string& dir_name, int64_t parent_file_id) {
    auto logger = get_logger("session");
    auto t0 = std::chrono::steady_clock::now();

    json data = {
        {"driveId", 0},
        {"etag", ""},
        {"fileName", dir_name},
        {"parentFileId", parent_file_id},
        {"size", 0},
        {"type", 1},
        {"duplicate", 1},
        {"NotReuse", true},
        {"event", "newCreateFolder"},
        {"operateType", 1},
    };

    cpr::Response resp;
    try {
        resp = _http.Post(
            cpr::Url{"https://www.123pan.cn/a/api/file/upload_request"},
            cpr::Body{data.dump()},
            cpr::Timeout{10000}
        );
    } catch (const std::exception& e) {
        logger->error("创建文件夹失败: name={}, parent={}, err={}", dir_name, parent_file_id, e.what());
        return ApiReturnModel(-1, -1, ApiCode::FAIL, e.what());
    }

    auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    auto [body, error] = safe_json(resp);
    if (error) return *error;

    int code = body.value("code", -1);
    logger->debug("创建文件夹 ({:.2f}s): name={}, parent={}, code={}", elapsed, dir_name, parent_file_id, code);

    if (code != 0) {
        logger->error("创建文件夹失败: name={}, code={}, msg={}", dir_name, code, body.value("message", ""));
        return ApiReturnModel(code, code, ApiCode::FAIL, body.value("message", ""));
    }

    return ApiReturnModel(0, 200, ApiCode::SUCCESS, "", body["data"]);
}

// ============================================================
// Trash / Restore
// ============================================================
ApiReturnModel NetSession::trash_file(const json& file_info, bool operation) {
    auto logger = get_logger("session");
    auto t0 = std::chrono::steady_clock::now();

    json data = {
        {"driveId", 0},
        {"fileTrashInfoList", file_info},
        {"operation", operation},
    };

    std::string op_name = operation ? "删除" : "恢复";
    std::string file_name = "?";
    if (file_info.is_object()) {
        file_name = file_info.value("FileName", file_info.value("fileName", "?"));
    }

    cpr::Response resp;
    try {
        resp = _http.Post(
            cpr::Url{"https://www.123pan.cn/a/api/file/trash"},
            cpr::Body{data.dump()},
            cpr::Timeout{10000}
        );
    } catch (const std::exception& e) {
        logger->error("{}文件失败: name={}, err={}", op_name, file_name, e.what());
        return ApiReturnModel(-1, -1, ApiCode::FAIL, e.what());
    }

    auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    auto [body, error] = safe_json(resp);
    if (error) return *error;

    int code = body.value("code", -1);
    logger->debug("{}文件 ({:.2f}s): name={}, code={}", op_name, elapsed, file_name, code);

    if (code != 0) {
        logger->error("{}文件失败: name={}, code={}, msg={}", op_name, file_name, code, body.value("message", ""));
        return ApiReturnModel(code, code, ApiCode::FAIL, body.value("message", ""));
    }

    return ApiReturnModel(0, 200, ApiCode::SUCCESS, body.value("message", ""));
}

ApiReturnModel NetSession::restore_file(const json& file_info) {
    return trash_file(file_info, false);
}

// ============================================================
// Get file download link
// ============================================================
ApiReturnModel NetSession::get_file_link(const json& file_info) {
    auto logger = get_logger("session");
    auto t0 = std::chrono::steady_clock::now();

    int type_val = 0;
    if (file_info.is_object()) {
        type_val = file_info.value("Type", file_info.value("type", 0));
    }

    std::string url;
    json request_data;

    if (type_val == 1) {
        // Folder: batch download
        url = "https://www.123pan.cn/a/api/file/batch_download_info";
        int64_t fid = file_info.value("FileId", file_info.value("fileId", 0));
        request_data["fileIdList"] = json::array({json::object({{"fileId", fid}})});
    } else {
        // File: single download info
        url = "https://www.123pan.cn/a/api/file/download_info";
        request_data = {
            {"driveId", 0},
            {"etag", file_info.value("Etag", file_info.value("etag", ""))},
            {"fileId", file_info.value("FileId", file_info.value("fileId", 0))},
            {"s3keyFlag", file_info.value("S3KeyFlag", file_info.value("s3keyFlag", ""))},
            {"type", type_val},
            {"fileName", file_info.value("FileName", file_info.value("fileName", ""))},
            {"size", file_info.value("Size", file_info.value("size", 0))},
        };
    }

    std::string file_name = request_data.value("fileName", "?");

    cpr::Response resp;
    try {
        resp = _http.Post(
            cpr::Url{url},
            cpr::Body{request_data.dump()},
            cpr::Timeout{10000}
        );
    } catch (const std::exception& e) {
        logger->error("获取下载链接失败: name={}, err={}", file_name, e.what());
        return ApiReturnModel(-1, -1, ApiCode::FAIL, e.what());
    }

    auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    auto [body, error] = safe_json(resp);
    if (error) return *error;

    int code = body.value("code", -1);
    logger->debug("获取下载链接 ({:.2f}s): name={}, type={}, code={}", elapsed, file_name, type_val, code);

    // Check for download limit codes (5113, 5114)
    if (code != 0 && (code == 5113 || code == 5114)) {
        logger->warn("下载流量已超出限制 (code={})，已绕过拦截: name={}", code, file_name);
        // Continue to URL rewrite bypass
    } else if (code != 0) {
        logger->error("获取下载链接失败: name={}, code={}, msg={}", file_name, code, body.value("message", ""));
        return ApiReturnModel(code, code, ApiCode::FAIL, body.value("message", ""));
    }

    auto& data = body["data"];

    // Check for direct redirect URL
    std::string redirect_url = data.value("RedirectUrl", data.value("redirect_url", ""));
    if (!redirect_url.empty()) {
        logger->info("下载链接已获取（直链）: name={}", file_name);
        return ApiReturnModel(0, 200, ApiCode::SUCCESS, "", redirect_url);
    }

    // Get DownloadUrl and rewrite via web-pro2 proxy
    std::string download_url = data.value("DownloadUrl", data.value("downloadUrl", ""));
    if (download_url.empty()) {
        logger->error("响应中未找到下载链接: name={}", file_name);
        return ApiReturnModel(-1, -1, ApiCode::FAIL, "响应中未找到下载链接");
    }

    std::string rewritten = rewrite_download_url(download_url);
    std::string resolved = resolve_download_url(rewritten);
    logger->info("下载链接已获取: name={}", file_name);
    return ApiReturnModel(0, 200, ApiCode::SUCCESS, "", resolved);
}

// ============================================================
// Download URL rewriting (bypass traffic limit)
// ============================================================
std::string NetSession::rewrite_download_url(const std::string& url) {
    auto logger = get_logger("session");
    try {
        // Parse URL components
        auto q_pos = url.find('?');
        std::string base_url = (q_pos != std::string::npos) ? url.substr(0, q_pos) : url;
        std::string query_str = (q_pos != std::string::npos) ? url.substr(q_pos + 1) : "";

        // Simple URL parsing
        auto proto_end = base_url.find("://");
        std::string protocol = (proto_end != std::string::npos) ? base_url.substr(0, proto_end) : "https";
        std::string rest = (proto_end != std::string::npos) ? base_url.substr(proto_end + 3) : base_url;
        auto path_start = rest.find('/');
        std::string host = (path_start != std::string::npos) ? rest.substr(0, path_start) : rest;
        std::string path = (path_start != std::string::npos) ? rest.substr(path_start) : "/";

        if (host.find("web-pro") != std::string::npos) {
            // Already web-pro domain, decode params -> add auto_redirect -> re-encode
            std::string params_b64;
            // Parse query parameters
            std::regex param_regex("([^&=]+)=([^&]*)");
            auto words_begin = std::sregex_iterator(query_str.begin(), query_str.end(), param_regex);
            auto words_end = std::sregex_iterator();
            for (auto i = words_begin; i != words_end; ++i) {
                if ((*i)[1] == "params") {
                    params_b64 = (*i)[2];
                    break;
                }
            }

            if (!params_b64.empty()) {
                std::string decoded = b64_decode(params_b64);
                if (decoded.empty()) decoded = params_b64;

                // Parse inner URL
                auto inner_q = decoded.find('?');
                std::string inner_base = (inner_q != std::string::npos) ? decoded.substr(0, inner_q) : decoded;
                std::string inner_query = (inner_q != std::string::npos) ? decoded.substr(inner_q + 1) : "";

                // Add auto_redirect=0
                if (!inner_query.empty()) inner_query += "&";
                inner_query += "auto_redirect=0";

                std::string new_inner = (inner_q != std::string::npos)
                    ? inner_base + "?" + inner_query
                    : decoded + "?auto_redirect=0";

                std::string new_params = b64_encode(new_inner);
                return std::format("https://{}{}?params={}&is_s3=0", host, path, new_params);
            }
            return url;
        } else {
            // Non web-pro domain, rewrite to web-pro2
            // Add auto_redirect=0 to original URL
            std::string new_query = query_str;
            if (!new_query.empty()) new_query += "&";
            new_query += "auto_redirect=0";

            std::string rewritten_orig = base_url + "?" + new_query;
            std::string encoded_params = b64_encode(rewritten_orig);

            logger->debug("下载 URL 已重写到 web-pro2 代理");
            return std::format("https://web-pro2.123952.com/download-v2/?params={}&is_s3=0", encoded_params);
        }
    } catch (const std::exception& e) {
        logger->warn("下载 URL 重写失败，使用原始 URL: {}", e.what());
        return url;
    }
}

std::string NetSession::resolve_download_url(const std::string& url) {
    auto logger = get_logger("session");
    try {
        cpr::Response resp = cpr::Get(
            cpr::Url{url},
            cpr::Timeout{10000},
            cpr::Redirect{false}  // Don't follow redirects
        );

        // Check Location header
        if (resp.header.find("Location") != resp.header.end()) {
            std::string location = resp.header.at("Location");
            if (!location.empty() && (resp.status_code == 301 || resp.status_code == 302 ||
                                       resp.status_code == 303 || resp.status_code == 307 ||
                                       resp.status_code == 308)) {
                logger->debug("下载 URL 已通过 Location 头解析 (status={})", resp.status_code);
                return location;
            }
        }

        // Check HTML body for href links
        std::string text = resp.text.substr(0, 500);
        std::regex href_regex(R"(href='(https?://[^']+)')");
        std::smatch match;
        if (std::regex_search(text, match, href_regex)) {
            std::string resolved = match[1].str();
            logger->debug("下载 URL 已通过 href 解析");
            return resolved;
        }

        logger->debug("下载 URL 未找到重定向，返回原始 URL: status={}", resp.status_code);
    } catch (const std::exception& e) {
        logger->warn("解析下载 URL HTTP 请求失败: {}", e.what());
    }

    // Fallback: decode download-v2 params
    std::string decoded = decode_download_v2_params(url);
    if (!decoded.empty()) {
        logger->debug("下载 URL 已通过 base64 params 解码");
        return decoded;
    }

    return url;
}

std::string NetSession::decode_download_v2_params(const std::string& url) {
    try {
        if (url.find("/download-v2/") == std::string::npos) return "";

        auto q_pos = url.find('?');
        if (q_pos == std::string::npos) return "";
        std::string query_str = url.substr(q_pos + 1);

        std::regex param_regex("([^&=]+)=([^&]*)");
        auto words_begin = std::sregex_iterator(query_str.begin(), query_str.end(), param_regex);
        auto words_end = std::sregex_iterator();
        std::string params_b64;
        for (auto i = words_begin; i != words_end; ++i) {
            if ((*i)[1] == "params") {
                params_b64 = (*i)[2];
                break;
            }
        }

        if (params_b64.empty()) return "";
        std::string decoded = b64_decode(params_b64);
        if (decoded.starts_with("http")) return decoded;
    } catch (...) {}

    return "";
}

// ============================================================
// Rename
// ============================================================
ApiReturnModel NetSession::rename_file(int64_t file_id, const std::string& new_name) {
    auto logger = get_logger("session");
    auto t0 = std::chrono::steady_clock::now();

    json data = {
        {"driveId", 0},
        {"fileId", file_id},
        {"fileName", new_name},
    };

    cpr::Response resp;
    try {
        resp = _http.Post(
            cpr::Url{"https://www.123pan.cn/a/api/file/rename"},
            cpr::Body{data.dump()},
            cpr::Timeout{10000}
        );
    } catch (const std::exception& e) {
        logger->error("重命名失败: file_id={}, new_name={}, err={}", file_id, new_name, e.what());
        return ApiReturnModel(-1, -1, ApiCode::FAIL, e.what());
    }

    auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    auto [body, error] = safe_json(resp);
    if (error) return *error;

    int code = body.value("code", -1);
    logger->debug("重命名 ({:.2f}s): file_id={}, new_name={}, code={}", elapsed, file_id, new_name, code);

    if (code != 0) {
        logger->error("重命名失败: file_id={}, code={}, msg={}", file_id, code, body.value("message", ""));
        return ApiReturnModel(code, code, ApiCode::FAIL, body.value("message", ""));
    }

    return ApiReturnModel(0, 200, ApiCode::SUCCESS, "");
}

// ============================================================
// Download (multi-thread / single-thread)
// ============================================================
bool NetSession::check_range_support(const std::string& url) {
    auto logger = get_logger("session");
    try {
        cpr::Response resp = cpr::Head(
            cpr::Url{url},
            cpr::Timeout{5000},
            cpr::Redirect{true}
        );
        std::string accept_ranges;
        if (resp.header.find("Accept-Ranges") != resp.header.end()) {
            accept_ranges = resp.header.at("Accept-Ranges");
        }
        bool supports = (accept_ranges == "bytes");
        logger->debug("Range 支持检查: Accept-Ranges={} -> {}", accept_ranges, supports);
        return supports;
    } catch (const std::exception& e) {
        logger->debug("Range 检查 HEAD 请求失败: {}", e.what());
        return false;
    }
}

std::string NetSession::resolve_json_redirect_url(const std::string& url) {
    auto logger = get_logger("session");
    try {
        cpr::Response resp = cpr::Get(
            cpr::Url{url},
            cpr::Header{{"Range", "bytes=0-0"}},
            cpr::Timeout{8000},
            cpr::Redirect{true}
        );
        std::string redirect = check_json_redirect(resp);
        if (!redirect.empty()) {
            logger->info("预检发现 JSON 重定向，切换到真实下载链接");
            return redirect;
        }
    } catch (const std::exception& e) {
        logger->debug("JSON 重定向预检请求失败: {}", e.what());
    }
    return "";
}

std::string NetSession::check_json_redirect(const cpr::Response& resp) {
    std::string content_type;
    if (resp.header.find("Content-Type") != resp.header.end()) {
        content_type = resp.header.at("Content-Type");
    }

    if (content_type.find("json") == std::string::npos) return "";

    try {
        json body = json::parse(resp.text);
        int code = body.value("code", -1);
        if (code != 0) return "";

        std::string redirect_url;
        if (body.contains("data") && body["data"].is_object()) {
            redirect_url = body["data"].value("RedirectUrl",
                            body["data"].value("redirect_url", ""));
        }

        if (!redirect_url.empty() && redirect_url.starts_with("http")) {
            return redirect_url;
        }
    } catch (...) {}

    return "";
}

bool NetSession::download_single(
    const std::string& url,
    const fs::path& file_path,
    int64_t file_size,
    ProgressCallback progress_callback,
    int redirect_count
) {
    auto logger = get_logger("session");

    if (redirect_count >= 3) {
        logger->error("JSON 重定向次数过多，放弃下载: {}", file_path.filename().string());
        return false;
    }

    fs::path temp_path = file_path;
    temp_path += ".tmp";
    logger->debug("单线程下载开始: {}", file_path.filename().string());

    constexpr int max_conn_retries = 3;
    for (int conn_attempt = 0; conn_attempt < max_conn_retries; ++conn_attempt) {
        auto t0 = std::chrono::steady_clock::now();
        try {
            cpr::Response resp = cpr::Get(
                cpr::Url{url},
                cpr::Timeout{60000},
                cpr::ConnectTimeout{10000}
            );

            if (resp.status_code != 200) {
                throw std::runtime_error(std::format("HTTP {}", resp.status_code));
            }

            // Check for JSON redirect
            std::string content_type;
            if (resp.header.find("Content-Type") != resp.header.end()) {
                content_type = resp.header.at("Content-Type");
            }

            if (content_type.find("json") != std::string::npos) {
                try {
                    json body = json::parse(resp.text);
                    std::string redirect_url = body.value("data", json{}).value("RedirectUrl",
                                               body.value("data", json{}).value("redirect_url", ""));
                    if (!redirect_url.empty() && redirect_url.starts_with("http")) {
                        logger->info("单线程下载遇到 JSON 重定向: {} -> {} ...",
                                    file_path.filename().string(), redirect_url.substr(0, 80));
                        return download_single(redirect_url, file_path, file_size,
                                               progress_callback, redirect_count + 1);
                    }
                    std::string msg = body.value("message", body.value("msg", "未知错误"));
                    logger->error("CDN 返回 JSON 错误: {}", body.dump());
                    throw std::runtime_error(std::format("下载 {} 失败，CDN 返回: {}", file_path.filename().string(), msg));
                } catch (const json::parse_error&) {
                    // Not JSON, proceed with file download
                }
            }

            // Download to temp file
            std::ofstream out_file(temp_path, std::ios::binary);
            if (!out_file) {
                throw std::runtime_error("无法创建临时文件: " + temp_path.string());
            }

            const auto& data = resp.text;  // raw response body
            int64_t total_downloaded = 0;
            auto last_report = std::chrono::steady_clock::now();

            // Write downloaded data
            out_file.write(data.data(), data.size());
            total_downloaded = data.size();

            if (_download_limiter) {
                double wait = _download_limiter->consume(data.size());
                if (wait > 0) {
                    std::this_thread::sleep_for(std::chrono::duration<double>(wait));
                }
            }

            if (progress_callback) {
                progress_callback(total_downloaded, file_size > 0 ? file_size : total_downloaded);
            }

            out_file.close();

            auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            double size_mb = static_cast<double>(total_downloaded) / 1024.0 / 1024.0;
            logger->info("单线程下载完成: {} ({:.2f} MB / {:.1f}s)",
                        file_path.filename().string(), size_mb, elapsed);

            // Rename temp to final
            if (fs::exists(temp_path)) {
                if (fs::exists(file_path)) {
                    fs::remove(file_path);
                }
                fs::rename(temp_path, file_path);
            }
            return true;
        } catch (const std::exception& e) {
            auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            if (conn_attempt < max_conn_retries - 1) {
                double wait = (conn_attempt + 1) * 2.0;
                logger->warn("单线程下载连接中断 (第 {}/{} 次): {} ({:.1f}s)，{:.0f}s 后重试",
                            conn_attempt + 1, max_conn_retries,
                            file_path.filename().string(), elapsed, wait);
                if (fs::exists(temp_path)) fs::remove(temp_path);
                std::this_thread::sleep_for(std::chrono::duration<double>(wait));
                continue;
            }
            logger->error("单线程下载失败（连接中断 {} 次）: {} ({:.1f}s): {}",
                         max_conn_retries, file_path.filename().string(), elapsed, e.what());
            if (fs::exists(temp_path)) fs::remove(temp_path);
            throw;
        }
    }
    return false;
}

bool NetSession::download_chunked(
    const std::string& url,
    const fs::path& file_path,
    int64_t file_size,
    ProgressCallback progress_callback
) {
    auto logger = get_logger("session");

    int num_threads = _num_threads;
    int64_t chunk_size = std::max(_chunk_size, file_size / num_threads);

    fs::path temp_path = file_path;
    temp_path += ".tmp";

    std::mutex progress_mutex;
    int64_t downloaded_bytes = 0;
    auto last_report = std::chrono::steady_clock::now();
    std::mutex error_mutex;
    std::vector<std::string> errors;

    // Calculate chunk ranges
    struct ChunkRange { int64_t start; int64_t end; int index; };
    std::vector<ChunkRange> ranges;
    int64_t start = 0;
    int index = 0;
    while (start < file_size) {
        int64_t end = std::min(start + chunk_size - 1, file_size - 1);
        ranges.push_back({start, end, index});
        start = end + 1;
        ++index;
    }

    auto download_chunk = [&](const ChunkRange& r) -> bool {
        fs::path part_path = temp_path.string() + std::format(".part{}", r.index);
        auto headers = cpr::Header{{"Range", std::format("bytes={}-{}", r.start, r.end)}};

        constexpr int max_retries = 3;
        for (int attempt = 0; attempt < max_retries; ++attempt) {
            try {
                if (fs::exists(part_path)) fs::remove(part_path);

                cpr::Response resp = cpr::Get(
                    cpr::Url{url},
                    headers,
                    cpr::Timeout{60000},
                    cpr::ConnectTimeout{10000}
                );

                if (resp.status_code != 200 && resp.status_code != 206) {
                    throw std::runtime_error(std::format("HTTP {}", resp.status_code));
                }

                std::ofstream out(part_path, std::ios::binary);
                if (!out) throw std::runtime_error("Cannot create part file");
                out.write(resp.text.data(), resp.text.size());
                out.close();

                // Apply speed limit
                if (_download_limiter) {
                    double wait = _download_limiter->consume(static_cast<int64_t>(resp.text.size()));
                    if (wait > 0) {
                        std::this_thread::sleep_for(std::chrono::duration<double>(wait));
                    }
                }

                // Update progress
                {
                    std::lock_guard<std::mutex> lock(progress_mutex);
                    downloaded_bytes += static_cast<int64_t>(resp.text.size());
                    auto now = std::chrono::steady_clock::now();
                    if (progress_callback &&
                        std::chrono::duration<double>(now - last_report).count() >= 0.1) {
                        progress_callback(downloaded_bytes, file_size);
                        last_report = now;
                    }
                }
                return true;
            } catch (const std::exception& e) {
                if (fs::exists(part_path)) {
                    try { fs::remove(part_path); } catch (...) {}
                }
                if (attempt < max_retries - 1) {
                    double wait = (attempt + 1) * 1.0;
                    logger->warn("分片 {} 第 {} 次失败，{:.0f}s 后重试: {}", r.index, attempt + 1, wait, e.what());
                    std::this_thread::sleep_for(std::chrono::duration<double>(wait));
                    continue;
                }
                std::lock_guard<std::mutex> lock(error_mutex);
                errors.push_back(std::format("分片{}: {}", r.index, e.what()));
                return false;
            }
        }
        return false;
    };

    // Launch download threads
    std::vector<std::future<bool>> futures;
    for (auto& r : ranges) {
        futures.push_back(std::async(std::launch::async, download_chunk, r));
    }

    // Wait for all chunks
    bool all_success = true;
    for (auto& f : futures) {
        if (!f.get()) all_success = false;
    }

    if (!all_success) {
        // Clean up parts
        for (auto& r : ranges) {
            fs::path p = temp_path.string() + std::format(".part{}", r.index);
            if (fs::exists(p)) {
                try { fs::remove(p); } catch (...) {}
            }
        }
        std::string err_msg;
        for (auto& e : errors) { err_msg += e + "; "; }
        throw std::runtime_error("分片下载失败: " + err_msg);
    }

    // Merge parts
    {
        std::ofstream out(temp_path, std::ios::binary);
        if (!out) throw std::runtime_error("Cannot create merged file");

        for (auto& r : ranges) {
            fs::path p = temp_path.string() + std::format(".part{}", r.index);
            std::ifstream in(p, std::ios::binary);
            if (!in) throw std::runtime_error("Cannot read part file " + std::to_string(r.index));
            out << in.rdbuf();
            in.close();
            fs::remove(p);
        }
    }

    // Rename to final
    if (fs::exists(temp_path)) {
        if (fs::exists(file_path)) fs::remove(file_path);
        fs::rename(temp_path, file_path);
    }

    return true;
}

bool NetSession::download_file_multithread(
    const std::string& url,
    const fs::path& file_path,
    int64_t file_size,
    ProgressCallback progress_callback
) {
    auto logger = get_logger("session");

    logger->info("下载文件: {} ({:.2f} MB), multi={}, threads={}",
                file_path.filename().string(),
                static_cast<double>(file_size) / 1024.0 / 1024.0,
                _multi_thread_enabled, _num_threads);

    if (file_size == 0) {
        logger->info("空文件，跳过下载: {}", file_path.filename().string());
        fs::create_directories(file_path.parent_path());
        std::ofstream(file_path).close();  // Create empty file
        if (progress_callback) progress_callback(0, 0);
        return true;
    }

    // Single thread path
    if (!_multi_thread_enabled || _num_threads <= 1) {
        logger->debug("多线程已禁用，使用单线程");
        return download_single(url, file_path, file_size, progress_callback);
    }

    if (file_size < 5 * 1024 * 1024) {  // 5MB
        logger->debug("文件小于 5MB，回退单线程");
        return download_single(url, file_path, file_size, progress_callback);
    }

    // Multi-thread path: resolve redirect and check Range
    std::string resolved_url = url;
    auto json_redirect = resolve_json_redirect_url(url);
    if (!json_redirect.empty()) {
        resolved_url = json_redirect;
    }

    bool supports_range = check_range_support(resolved_url);
    if (!supports_range) {
        logger->debug("服务器不支持 Range，回退单线程");
        return download_single(resolved_url, file_path, file_size, progress_callback);
    }

    logger->debug("启用多线程分片下载: {} 线程", _num_threads);
    return download_chunked(resolved_url, file_path, file_size, progress_callback);
}

// ============================================================
// Utility functions
// ============================================================
std::string format_file_size(int64_t size) {
    constexpr const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    double s = static_cast<double>(size);

    for (size_t i = 0; i < std::size(units); ++i) {
        if (s < 1024.0) {
            // Format with appropriate precision
            if (s < 10.0) {
                return std::format("{:.2f} {}", s, units[i]);
            } else if (s < 100.0) {
                return std::format("{:.1f} {}", s, units[i]);
            } else {
                return std::format("{:.0f} {}", s, units[i]);
            }
        }
        s /= 1024.0;
    }
    return std::format("{:.2f} {}", s, units[std::size(units) - 1]);
}

bool check_version() {
    auto logger = get_logger("api");
    try {
        cpr::Response resp = cpr::Get(
            cpr::Url{"https://api.github.com/repos/123pannextgen/123pan-cpp/releases/latest"},
            cpr::Timeout{5000}
        );
        if (resp.status_code != 200) return false;
        json info = json::parse(resp.text);
        std::string latest = info.value("name", "");
        std::string current = "v" + std::string(VERSION);
        return latest == current;
    } catch (const std::exception& e) {
        logger->error("获取版本信息出错: {}", e.what());
        return false;
    }
}

}  // namespace app
