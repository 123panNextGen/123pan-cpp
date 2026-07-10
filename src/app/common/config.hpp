#pragma once

#include <nlohmann/json.hpp>
#include <filesystem>
#include <string>
#include <vector>
#include <optional>

namespace app {

using json = nlohmann::json;

/// 获取配置文件目录路径。
std::filesystem::path get_config_dir();

/// 获取配置文件完整路径。
std::filesystem::path get_config_file_path();

/// 检查是否为 Windows 11。
bool is_win11();

/// 配置管理类（静态方法，与 Python ConfigManager 接口一致）。
class ConfigManager {
public:
    /// 确保配置目录存在。
    static void ensure_config_dir();

    /// 加载配置，不存在时返回默认配置。
    /// 自动迁移旧版本配置格式。
    static json load_config();

    /// 保存配置到文件。
    static bool save_config(const json& config);

    /// 获取当前登录的账户名。
    static std::string get_current_account_name();

    /// 获取指定账户信息。
    /// @param user_name 账户名，为空则获取当前账户。
    /// 自动解密密码（若已加密）。
    static json get_account(const std::string& user_name = "");

    /// 获取所有已保存账户名列表。
    static std::vector<std::string> get_account_names();

    /// 保存账户信息。
    /// @param user_name 账户名
    /// @param account_info 账户信息 JSON
    /// @param set_current 是否设为当前账户
    static bool save_account(
        const std::string& user_name,
        const json& account_info,
        bool set_current = true
    );

    /// 切换当前账户。
    static bool set_current_account(const std::string& user_name);

    /// 读取设置项。
    template<typename T>
    static T get_setting(const std::string& key, const T& default_value) {
        auto config = load_config();
        if (!config.contains("settings")) return default_value;
        auto& settings = config["settings"];
        if (!settings.contains(key)) return default_value;
        try {
            return settings[key].get<T>();
        } catch (...) {
            return default_value;
        }
    }

    /// 读取设置项（json 类型，用于嵌套结构）。
    static json get_setting_json(const std::string& key, const json& default_value = json{});

    /// 写入设置项。
    static bool set_setting(const std::string& key, const json& value);

private:
    ConfigManager() = delete;
};

}  // namespace app
