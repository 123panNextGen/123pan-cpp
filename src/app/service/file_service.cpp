#include "app/service/file_service.hpp"
#include "app/api/session.hpp"
#include "app/common/log.hpp"

#include <chrono>
#include <thread>
#include <algorithm>

using namespace std::chrono_literals;

namespace app {

FileService::FileService(std::shared_ptr<NetSession> session)
    : _session(std::move(session)) {}

std::tuple<int, std::vector<json>, int, bool, int>
FileService::get_dir_by_id(int64_t file_id, int page, int list_len,
                            bool all, int limit) {
    auto logger = get_logger("file");
    int get_pages = 3;
    int start_page = page * get_pages + 1;
    std::vector<json> lists;

    int total = -1;
    int times = 0;
    int length_now = list_len;
    auto t0 = std::chrono::steady_clock::now();

    if (all) {
        start_page = 1;
        length_now = 0;
    }

    while ((length_now < total || total == -1) && (times < get_pages || all)) {
        auto result = _session->get_file_list(file_id, false, false,
                                               start_page, limit, false);

        if (result.code == 2) {
            logger->warn("token 过期: file_id={}", file_id);
            return {result.code, {}, 0, false, times};
        }

        if (result.code != 0) {
            logger->error("获取文件列表失败: file_id={}, code={}, msg={}",
                         file_id, result.code, result.msg);
            return {result.code, {}, 0, false, times};
        }

        // Parse FileListResponse from result.data
        FileListResponse flr;
        try {
            flr = FileListResponse::from_json(result.data);
        } catch (...) {
            logger->error("解析文件列表数据失败: file_id={}", file_id);
            return {-1, {}, 0, false, times};
        }

        for (auto& item : flr.data.info_list) {
            lists.push_back(item.to_json());
        }

        total = flr.data.total;
        length_now += static_cast<int>(lists.size());
        ++start_page;
        ++times;

        logger->debug("分页加载: page={}, got={}, total={}, accumulated={}",
                     start_page - 1, lists.size(), total, length_now);

        if (times % 5 == 0) {
            logger->warn("文件夹内文件过多：{}/{}", length_now, total);
            logger->info("暂停3秒防止对服务器造成影响");
            std::this_thread::sleep_for(3s);
        }
    }

    auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    logger->info("目录列表加载完成: file_id={}, total={}, pages={}, {:.1f}s",
                file_id, total, times, elapsed);

    bool all_file = (length_now >= total);
    if (!all_file) {
        logger->warn("文件夹内文件过多：{}/{}，未完全加载", length_now, total);
    }

    return {0, lists, total, all_file, times};
}

std::pair<int64_t, std::string>
FileService::mkdir(const std::string& dirname,
                   const std::vector<json>& file_list,
                   int64_t parent_file_id, bool remakedir) {
    auto logger = get_logger("file");

    if (!remakedir) {
        for (auto& item : file_list) {
            std::string name = item.value("FileName", item.value("fileName", ""));
            if (name == dirname) {
                logger->info("文件夹已存在");
                return {item.value("FileId", item.value("fileId", 0)), ""};
            }
        }
    }

    auto result = _session->create_dir(dirname, parent_file_id);
    if (result.code != 0) {
        logger->error("创建文件夹失败: {}", result.msg);
        return {0, result.msg};
    }

    try {
        int64_t file_id = result.data["Info"]["FileId"].get<int64_t>();
        logger->info("创建成功: {}", file_id);
        return {file_id, ""};
    } catch (const std::exception& e) {
        logger->error("创建文件夹解析失败: {}", e.what());
        return {0, e.what()};
    }
}

std::pair<int64_t, std::string>
FileService::create_folder(const std::string& dirname, int64_t parent_file_id) {
    auto result = _session->create_dir(dirname, parent_file_id);
    if (result.code != 0) {
        return {0, result.msg};
    }
    try {
        int64_t file_id = result.data["Info"]["FileId"].get<int64_t>();
        return {file_id, ""};
    } catch (const std::exception& e) {
        return {0, e.what()};
    }
}

std::pair<bool, std::string>
FileService::delete_file(std::vector<json>& file_list, int file_index,
                          bool by_num, bool operation) {
    auto logger = get_logger("file");

    json file_detail;
    if (by_num) {
        if (file_index < 0 || file_index >= static_cast<int>(file_list.size())) {
            throw std::out_of_range("文件索引超出范围");
        }
        file_detail = file_list[file_index];
    } else {
        // file_index is actually file_id lookup... this is a simplified version
        // In the original Python, by_num=False searches by file object
        // For now, just use by index
        if (file_index >= 0 && file_index < static_cast<int>(file_list.size())) {
            file_detail = file_list[file_index];
        } else {
            throw std::runtime_error("文件不存在");
        }
    }

    auto result = _session->trash_file(file_detail, operation);
    logger->debug("删除文件响应: code={}, msg={}", result.code, result.msg);

    if (result.code != 0) {
        logger->error("删除文件失败: {}", result.msg);
        return {false, result.msg};
    }

    // Remove from list
    if (by_num) {
        file_list.erase(file_list.begin() + file_index);
    }

    return {true, result.msg};
}

bool FileService::rename_file(int64_t file_id, const std::string& new_name) {
    auto logger = get_logger("file");
    auto result = _session->rename_file(file_id, new_name);
    logger->debug("重命名文件响应: code={}, msg={}", result.code, result.msg);

    if (result.code != 0) {
        logger->error("重命名失败: {}", result.msg);
        return false;
    }
    logger->info("重命名成功: {}", new_name);
    return true;
}

std::pair<bool, std::string>
FileService::delete_file_by_id(int64_t file_id, int64_t parent_file_id) {
    auto [code, items, total, all_file, pages] =
        get_dir_by_id(parent_file_id, 0, 0, true, 1000);
    if (code != 0) {
        return {false, "获取文件列表失败"};
    }

    for (size_t i = 0; i < items.size(); ++i) {
        if (std::to_string(items[i].value("FileId", items[i].value("fileId", 0))) ==
            std::to_string(file_id)) {
            return delete_file(items, static_cast<int>(i), true, true);
        }
    }

    return {false, "文件未找到"};
}

std::vector<json> FileService::recycle() {
    auto logger = get_logger("file");
    auto result = _session->get_trash_list();
    if (result.code != 0) {
        logger->error("获取回收站失败: code={}, msg={}", result.code, result.msg);
        return {};
    }

    try {
        FileListResponse flr = FileListResponse::from_json(result.data);
        std::vector<json> items;
        for (auto& item : flr.data.info_list) {
            items.push_back(item.to_json());
        }
        return items;
    } catch (...) {
        return {};
    }
}

std::string FileService::share(const std::vector<int64_t>& file_id_list,
                                const std::string& share_pwd) {
    if (file_id_list.empty()) {
        throw std::invalid_argument("文件ID列表为空");
    }

    json file_ids = json::array();
    for (auto fid : file_id_list) {
        file_ids.push_back(fid);
    }

    json data = {
        {"driveId", 0},
        {"expiration", "2099-12-12T08:00:00+08:00"},
        {"fileIdList", file_ids},
        {"shareName", "123云盘分享"},
        {"sharePwd", share_pwd},
        {"event", "shareCreate"},
    };

    auto& http = _session->http_session();
    http.SetUrl(cpr::Url{"https://www.123pan.cn/a/api/share/create"});
    http.SetBody(cpr::Body{data.dump()});
    http.SetTimeout(cpr::Timeout{10000});
    cpr::Response resp = http.Post();

    try {
        json resp_json = json::parse(resp.text);
        if (resp_json.value("code", -1) != 0) {
            throw std::runtime_error("分享失败: " + resp_json.value("message", ""));
        }
        std::string share_key = resp_json["data"]["ShareKey"].get<std::string>();
        return "https://www.123pan.cn/s/" + share_key;
    } catch (const json::parse_error&) {
        throw std::runtime_error("分享响应解析失败");
    }
}

std::tuple<std::vector<json>, std::set<int64_t>, std::map<int64_t, std::string>>
FileService::get_all_things(int64_t file_id, std::set<int64_t> dir_ids,
                             std::vector<json> file_list,
                             std::map<int64_t, std::string> name_dict) {
    dir_ids.erase(file_id);
    auto [code, items, total, all_file, pages] =
        get_dir_by_id(file_id, 0, 0, true, 100);

    if (code != 0) {
        return {file_list, dir_ids, name_dict};
    }

    for (auto& item : items) {
        int type = item.value("Type", item.value("type", 0));
        if (type == 0) {
            file_list.push_back(item);
        } else {
            int64_t fid = item.value("FileId", item.value("fileId", 0));
            dir_ids.insert(fid);
            name_dict[fid] = item.value("FileName", item.value("fileName", ""));
        }
    }

    for (auto did : std::vector<int64_t>(dir_ids.begin(), dir_ids.end())) {
        if (did != file_id) {
            std::tie(file_list, dir_ids, name_dict) =
                get_all_things(did, std::move(dir_ids),
                               std::move(file_list), std::move(name_dict));
        }
    }

    return {file_list, dir_ids, name_dict};
}

}  // namespace app
