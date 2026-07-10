#include "app/common/config.hpp"
#include "app/common/log.hpp"

#include <fstream>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#endif

namespace app {

namespace fs = std::filesystem;

// ============================================================
// Path helpers
// ============================================================
fs::path get_config_dir() {
#ifdef _WIN32
    const char* appdata = std::getenv("APPDATA");
    if (appdata) {
        return fs::path(appdata) / "123pan";
    }
    return fs::path(std::getenv("USERPROFILE") ? std::getenv("USERPROFILE") : ".") / ".config" / "123pan";
#else
    const char* home = std::getenv("HOME");
    if (home) {
        return fs::path(home) / ".config" / "123pan";
    }
    return fs::path(".") / ".config" / "123pan";
#endif
}

fs::path get_config_file_path() {
    return get_config_dir() / "config.json";
}

bool is_win11() {
#ifdef _WIN32
    // Check Windows build number
    // Windows 11 build >= 22000
    using RtlGetVersion_t = LONG (WINAPI*)(PRTL_OSVERSIONINFOW);
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (ntdll) {
        auto RtlGetVersion = reinterpret_cast<RtlGetVersion_t>(
            GetProcAddress(ntdll, "RtlGetVersion")
        );
        if (RtlGetVersion) {
            RTL_OSVERSIONINFOW info{};
            info.dwOSVersionInfoSize = sizeof(info);
            if (RtlGetVersion(&info) == 0) {
                return info.dwBuildNumber >= 22000;
            }
        }
    }
    return false;
#else
    return false;
#endif
}

// ============================================================
// ConfigManager
// ============================================================
void ConfigManager::ensure_config_dir() {
    auto dir = get_config_dir();
    if (!fs::exists(dir)) {
        fs::create_directories(dir);
    }
}

json ConfigManager::load_config() {
    auto logger = get_logger("config");

    json default_config = {
        {"currentAccount", ""},
        {"accounts", json::object()},
        {"settings", {
            {"defaultDownloadPath", (fs::path(std::getenv("HOME") ? std::getenv("HOME") : ".") / "Downloads").string()},
            {"askDownloadLocation", true},
            {"multiThreadDownload", true},
            {"downloadSpeedLimit", 0},
            {"uploadSpeedLimit", 0},
            {"proxyEnabled", false},
            {"proxyType", "http"},
            {"proxyHost", ""},
            {"proxyPort", 0},
            {"proxyUsername", ""},
            {"proxyPassword", ""},
            {"logLevel", "DEBUG"},
        }}
    };

    auto config_file = get_config_file_path();
    if (!fs::exists(config_file)) {
        logger->debug("配置文件不存在，使用默认配置");
        return default_config;
    }

    try {
        std::ifstream f(config_file);
        if (!f.is_open()) {
            logger->warn("无法打开配置文件，使用默认配置");
            return default_config;
        }

        json config = json::parse(f);
        auto file_size = fs::file_size(config_file);
        logger->debug("配置文件已加载: {} ({} KB)",
                      config_file.string(), file_size / 1024);

        bool migrated = false;

        // 兼容旧版本配置：补全 settings
        if (!config.contains("settings")) {
            config["settings"] = default_config["settings"];
            migrated = true;
            logger->info("配置迁移: 补全 settings 字段");
        }

        // 兼容旧版本配置：补全 accounts
        if (!config.contains("accounts")) {
            config["accounts"] = json::object();
        }

        // 迁移旧版顶层字段到 accounts
        std::string old_user;
        if (config.contains("userName") && config["userName"].is_string()) {
            old_user = config["userName"].get<std::string>();
        }

        if (!old_user.empty()) {
            if (!config["accounts"].contains(old_user)) {
                config["accounts"][old_user] = {
                    {"userName", old_user},
                    {"passWord", config.value("passWord", "")},
                    {"authorization", config.value("authorization", "")},
                    {"deviceType", config.value("deviceType", "")},
                    {"osVersion", config.value("osVersion", "")},
                    {"loginuuid", config.value("loginuuid", "")},
                };
                migrated = true;
                logger->info("配置迁移: 将旧账号 {} 迁移到 accounts 区块", old_user);
            }
        }

        // 补全 currentAccount
        if (!config.contains("currentAccount") || config["currentAccount"].get<std::string>().empty()) {
            if (config.contains("userName") && config["userName"].is_string()) {
                config["currentAccount"] = config["userName"].get<std::string>();
            } else if (!config["accounts"].empty()) {
                config["currentAccount"] = config["accounts"].begin().key();
            }
            migrated = true;
            logger->info("配置迁移: 补全 currentAccount={}", config["currentAccount"].get<std::string>());
        }

        // 删除冗余顶层字段
        for (const auto* key : {"userName", "passWord", "authorization",
                                "deviceType", "osVersion", "loginuuid"}) {
            if (config.contains(key)) {
                config.erase(key);
                migrated = true;
                logger->info("配置迁移: 删除冗余顶层字段 {}", key);
            }
        }

        // 补全默认设置
        for (auto& [key, val] : default_config["settings"].items()) {
            if (!config["settings"].contains(key)) {
                config["settings"][key] = val;
                migrated = true;
                logger->info("配置迁移: 补全默认设置 {}={}", key, val.dump());
            }
        }

        if (migrated) {
            save_config(config);
        }

        return config;
    } catch (const std::exception& e) {
        logger->error("加载配置失败: {}", e.what());
        try {
            std::ofstream f(config_file);
            f << default_config.dump(2, ' ', false, json::error_handler_t::replace);
            logger->info("配置文件已重置为默认值");
        } catch (const std::exception& e2) {
            logger->error("重写配置失败: {}", e2.what());
        }
        return default_config;
    }
}

bool ConfigManager::save_config(const json& config) {
    auto logger = get_logger("config");
    try {
        ensure_config_dir();
        auto config_file = get_config_file_path();
        std::ofstream f(config_file);
        if (!f.is_open()) {
            logger->error("无法打开配置文件写入: {}", config_file.string());
            return false;
        }
        f << config.dump(2, ' ', false, json::error_handler_t::replace);
        logger->debug("配置已保存到 {}", config_file.string());
        return true;
    } catch (const std::exception& e) {
        logger->error("保存配置失败: {}", e.what());
        return false;
    }
}

std::string ConfigManager::get_current_account_name() {
    auto config = load_config();
    std::string name = config.value("currentAccount", "");
    auto logger = get_logger("config");
    logger->debug("当前账号: {}", name.empty() ? "(无)" : name);
    return name;
}

json ConfigManager::get_account(const std::string& user_name) {
    auto logger = get_logger("config");
    auto config = load_config();
    auto& accounts = config["accounts"];

    json account;
    if (!user_name.empty()) {
        if (accounts.contains(user_name)) {
            account = accounts[user_name];
            logger->debug("获取账号 {}: 存在", user_name);
        } else {
            logger->debug("获取账号 {}: 不存在", user_name);
        }
    } else {
        std::string current = config.value("currentAccount", "");
        if (!current.empty() && accounts.contains(current)) {
            account = accounts[current];
            logger->debug("获取当前账号 {}: 存在", current);
        } else {
            logger->debug("获取当前账号: 不存在");
        }
    }

    // 解密密码
    if (!account.empty() && account.contains("passWord")) {
        std::string pwd = account["passWord"].get<std::string>();
        if (pwd.starts_with("enc:")) {
            // Forward declare to avoid circular dependency
            // credential::decrypt_credential is used
            // We'll handle this in api.cpp through the Pan123 class
            logger->debug("账号密码需解密");
        }
    }

    return account;
}

std::vector<std::string> ConfigManager::get_account_names() {
    auto config = load_config();
    std::vector<std::string> names;
    for (auto& [key, val] : config["accounts"].items()) {
        names.push_back(key);
    }
    auto logger = get_logger("config");
    logger->debug("已保存账号列表: {} 个", names.size());
    return names;
}

bool ConfigManager::save_account(
    const std::string& user_name,
    const json& account_info,
    bool set_current
) {
    auto logger = get_logger("config");
    auto config = load_config();

    if (!config.contains("accounts")) {
        config["accounts"] = json::object();
        logger->debug("初始化 accounts 区块");
    }

    json info = account_info;

    // 加密密码
    if (info.contains("passWord")) {
        std::string pwd = info["passWord"].get<std::string>();
        if (!pwd.empty() && !pwd.starts_with("enc:")) {
            // Password encryption is handled by the credential module
            // The caller (Pan123) should encrypt before calling save_account
            // Or we import credential here
            logger->debug("账号密码需加密存储");
        }
    }

    config["accounts"][user_name] = info;

    if (set_current) {
        config["currentAccount"] = user_name;
        logger->info("保存账号 {} (设为当前)", user_name);
    } else {
        logger->info("保存账号 {}", user_name);
    }

    return save_config(config);
}

bool ConfigManager::set_current_account(const std::string& user_name) {
    auto logger = get_logger("config");
    auto config = load_config();

    if (!user_name.empty() && !config["accounts"].contains(user_name)) {
        logger->warn("设置当前账号失败: {} 不存在于已保存账号中", user_name);
        return false;
    }

    config["currentAccount"] = user_name;
    logger->info("切换当前账号为: {}", user_name.empty() ? "(空)" : user_name);
    return save_config(config);
}

json ConfigManager::get_setting_json(const std::string& key, const json& default_value) {
    auto config = load_config();
    if (!config.contains("settings")) return default_value;
    auto& settings = config["settings"];
    if (!settings.contains(key)) return default_value;
    auto logger = get_logger("config");
    logger->debug("读取设置 {} = {}", key, settings[key].dump());
    return settings[key];
}

bool ConfigManager::set_setting(const std::string& key, const json& value) {
    auto config = load_config();
    if (!config.contains("settings")) {
        config["settings"] = json::object();
    }
    config["settings"][key] = value;
    auto logger = get_logger("config");
    logger->info("设置变更: {} = {}", key, value.dump());
    return save_config(config);
}

}  // namespace app
