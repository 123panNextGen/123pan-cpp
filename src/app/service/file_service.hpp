#pragma once

#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <vector>
#include <tuple>
#include <set>

namespace app {

class NetSession;

/// 文件与目录管理服务。
class FileService {
public:
    explicit FileService(std::shared_ptr<NetSession> session);

    /// 按文件夹ID获取文件列表（支持分页）。
    /// @return (code, items, total, all_file, pages_read)
    std::tuple<int, std::vector<nlohmann::json>, int, bool, int>
    get_dir_by_id(int64_t file_id, int page = 0, int list_len = 0,
                  bool all = false, int limit = 100);

    /// 创建文件夹。
    /// @return (FileId, error_msg)
    std::pair<int64_t, std::string>
    mkdir(const std::string& dirname,
          const std::vector<nlohmann::json>& file_list,
          int64_t parent_file_id, bool remakedir = false);

    /// 创建文件夹（简化版，无需 file_list）。
    std::pair<int64_t, std::string>
    create_folder(const std::string& dirname, int64_t parent_file_id);

    /// 删除或恢复文件。
    /// @return (success, msg)
    std::pair<bool, std::string>
    delete_file(std::vector<nlohmann::json>& file_list, int file_index,
                bool by_num = true, bool operation = true);

    /// 重命名文件。
    bool rename_file(int64_t file_id, const std::string& new_name);

    /// 按文件ID删除文件。
    std::pair<bool, std::string>
    delete_file_by_id(int64_t file_id, int64_t parent_file_id);

    /// 获取回收站列表。
    std::vector<nlohmann::json> recycle();

    /// 创建分享链接。
    std::string share(const std::vector<int64_t>& file_id_list,
                      const std::string& share_pwd = "");

    /// 递归获取文件夹内所有内容。
    std::tuple<std::vector<nlohmann::json>, std::set<int64_t>, std::map<int64_t, std::string>>
    get_all_things(int64_t file_id, std::set<int64_t> dir_ids,
                   std::vector<nlohmann::json> file_list,
                   std::map<int64_t, std::string> name_dict);

private:
    std::shared_ptr<NetSession> _session;
};

}  // namespace app
