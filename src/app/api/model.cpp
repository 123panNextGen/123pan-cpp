#include "app/api/model.hpp"

#include <string>

namespace app {

// ============================================================
// Helper to safely get a value from JSON with fallback
// ============================================================
template<typename T>
static T safe_get(const json& j, const std::string& pascal_key,
                  const std::string& camel_key, T default_val) {
    if (j.contains(pascal_key)) {
        try { return j[pascal_key].get<T>(); } catch (...) {}
    }
    if (j.contains(camel_key)) {
        try { return j[camel_key].get<T>(); } catch (...) {}
    }
    return default_val;
}

static std::string safe_get_str(const json& j, const std::string& pascal_key,
                                 const std::string& camel_key,
                                 const std::string& default_val = "") {
    if (j.contains(pascal_key) && j[pascal_key].is_string()) {
        return j[pascal_key].get<std::string>();
    }
    if (j.contains(camel_key) && j[camel_key].is_string()) {
        return j[camel_key].get<std::string>();
    }
    return default_val;
}

// ============================================================
// FileItemModel
// ============================================================
int64_t FileItemModel::parse_timestamp(const json& value) {
    if (value.is_null()) return 0;
    if (value.is_number_integer()) return value.get<int64_t>();
    if (value.is_number_float()) return static_cast<int64_t>(value.get<double>());
    if (value.is_string()) {
        std::string s = value.get<std::string>();
        if (s.empty()) return 0;
        try {
            return static_cast<int64_t>(std::stod(s));
        } catch (...) {
            // Try ISO 8601 parsing (simplified)
            return 0;
        }
    }
    return 0;
}

FileItemModel FileItemModel::from_json(const json& j) {
    FileItemModel item;
    item.file_id = safe_get<int64_t>(j, "FileId", "fileId", 0);
    item.file_name = safe_get_str(j, "FileName", "fileName");
    item.type = safe_get<int>(j, "Type", "type", 0);
    item.size = safe_get<int64_t>(j, "Size", "size", 0);

    // Parse timestamps
    if (j.contains("CreateAt")) {
        item.create_at = parse_timestamp(j["CreateAt"]);
    } else if (j.contains("createAt")) {
        item.create_at = parse_timestamp(j["createAt"]);
    }

    if (j.contains("UpdateAt")) {
        item.update_at = parse_timestamp(j["UpdateAt"]);
    } else if (j.contains("updateAt")) {
        item.update_at = parse_timestamp(j["updateAt"]);
    }

    item.hidden = safe_get<bool>(j, "Hidden", "hidden", false);
    item.etag = safe_get_str(j, "Etag", "etag");
    item.s3key_flag = safe_get_str(j, "S3KeyFlag", "s3keyFlag");
    item.content_type = safe_get_str(j, "ContentType", "contentType");
    item.parent_file_id = safe_get<int64_t>(j, "ParentFileId", "parentFileId", 0);
    item.pin_yin = safe_get_str(j, "PinYin", "pinYin");
    item.starred_status = safe_get<bool>(j, "StarredStatus", "starredStatus", false);

    return item;
}

json FileItemModel::to_json() const {
    return {
        {"FileId", file_id},
        {"FileName", file_name},
        {"Type", type},
        {"Size", size},
        {"CreateAt", create_at},
        {"UpdateAt", update_at},
        {"Hidden", hidden},
        {"Etag", etag},
        {"S3KeyFlag", s3key_flag},
        {"ContentType", content_type},
        {"ParentFileId", parent_file_id},
        {"PinYin", pin_yin},
        {"StarredStatus", starred_status},
    };
}

// ============================================================
// FileListData
// ============================================================
FileListData FileListData::from_json(const json& j) {
    FileListData fld;
    fld.next = safe_get_str(j, "Next", "next", "-1");
    fld.len = safe_get<int>(j, "Len", "len", 0);
    fld.total = safe_get<int>(j, "Total", "total", 0);
    fld.is_first = safe_get<bool>(j, "IsFirst", "isFirst", false);

    // Parse InfoList
    json info_list;
    if (j.contains("InfoList")) {
        info_list = j["InfoList"];
    } else if (j.contains("infoList")) {
        info_list = j["infoList"];
    }

    if (info_list.is_array()) {
        for (const auto& item_json : info_list) {
            fld.info_list.push_back(FileItemModel::from_json(item_json));
        }
    }

    return fld;
}

// ============================================================
// FileListResponse
// ============================================================
FileListResponse FileListResponse::from_json(const json& j) {
    FileListResponse flr;
    flr.code = safe_get<int>(j, "code", "Code", -1);
    flr.message = safe_get_str(j, "message", "Message");

    json data_json;
    if (j.contains("data")) {
        data_json = j["data"];
    } else if (j.contains("Data")) {
        data_json = j["Data"];
    }

    if (!data_json.is_null()) {
        flr.data = FileListData::from_json(data_json);
    }

    return flr;
}

}  // namespace app
