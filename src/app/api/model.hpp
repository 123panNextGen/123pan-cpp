#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <cstdint>
#include <optional>
#include <chrono>

namespace app {

using json = nlohmann::json;

// ============================================================
// Enums
// ============================================================
enum class ApiCode {
    SUCCESS,
    FAIL
};

// ============================================================
// ApiReturnModel — API 调用返回模型
// ============================================================
struct ApiReturnModel {
    int code = -1;
    int api_code = -1;
    ApiCode api_code_enum = ApiCode::FAIL;
    std::string msg;
    json data;

    ApiReturnModel() = default;
    ApiReturnModel(int c, int ac, ApiCode ace, std::string m, json d = json{})
        : code(c), api_code(ac), api_code_enum(ace), msg(std::move(m)), data(std::move(d)) {}
};

// ============================================================
// DeviceModel — 设备信息
// ============================================================
struct DeviceModel {
    std::string os;
    std::string type;
};

// ============================================================
// UserInfoModel — 用户信息
// ============================================================
struct UserInfoModel {
    std::string user_name;
    std::string password;
    std::string uuid;
    std::string authorization;
    DeviceModel device;
};

// ============================================================
// FileItemModel — 文件/文件夹条目
// ============================================================
struct FileItemModel {
    int64_t file_id = 0;
    std::string file_name;
    int type = 0;       // 0=文件, 1=文件夹
    int64_t size = 0;
    int64_t create_at = 0;  // Unix timestamp
    int64_t update_at = 0;
    bool hidden = false;
    std::string etag;
    std::string s3key_flag;
    std::string content_type;
    int64_t parent_file_id = 0;
    std::string pin_yin;
    bool starred_status = false;

    /// 从 JSON 解析（兼容 PascalCase 和 camelCase）。
    static FileItemModel from_json(const json& j);

    /// 转换为 JSON（PascalCase 键名）。
    json to_json() const;

    /// 是否为文件夹。
    [[nodiscard]] bool is_dir() const { return type == 1; }

private:
    static int64_t parse_timestamp(const json& value);
};

// ============================================================
// FileListData — 文件列表数据
// ============================================================
struct FileListData {
    std::string next;
    int len = 0;
    int total = 0;
    bool is_first = false;
    std::vector<FileItemModel> info_list;

    static FileListData from_json(const json& j);
    json to_json() const;
};

// ============================================================
// FileListResponse — 文件列表 API 响应
// ============================================================
struct FileListResponse {
    int code = -1;
    std::string message;
    FileListData data;

    static FileListResponse from_json(const json& j);
    json to_json() const;
};

}  // namespace app
