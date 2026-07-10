#include "app/service/auth_service.hpp"
#include "app/api/session.hpp"
#include "app/common/config.hpp"
#include "app/common/log.hpp"
#include "app/common/credential.hpp"
#include "app/data/devices.hpp"

#include <random>

namespace app {

AuthService::AuthService(std::shared_ptr<NetSession> session)
    : _session(std::move(session))
{
    // Random device fingerprint
    static std::mt19937 rng(std::random_device{}());
    if (!ALL_DEVICE_TYPES.empty()) {
        std::uniform_int_distribution<size_t> dist(0, ALL_DEVICE_TYPES.size() - 1);
        devicetype = ALL_DEVICE_TYPES[dist(rng)];
    }
    if (!ALL_OS_VERSIONS.empty()) {
        std::uniform_int_distribution<size_t> dist(0, ALL_OS_VERSIONS.size() - 1);
        osversion = ALL_OS_VERSIONS[dist(rng)];
    }
    // Generate UUID
    std::uniform_int_distribution<int> hex_dist(0, 15);
    const char hex_chars[] = "0123456789abcdef";
    loginuuid.reserve(32);
    for (int i = 0; i < 32; ++i) {
        loginuuid.push_back(hex_chars[hex_dist(rng)]);
    }
}

void AuthService::sync_to_session() {
    UserInfoModel user_info;
    user_info.user_name = user_name;
    user_info.password = password;
    user_info.uuid = loginuuid;
    user_info.authorization = authorization;
    user_info.device.os = osversion;
    user_info.device.type = devicetype;
    _session->set_user_info(user_info);
}

int AuthService::login() {
    auto logger = get_logger("auth");
    logger->info("AuthService.login: user={}", user_name);

    auto result = _session->login(user_name, password);
    if (result.code != 200 && result.code != 0) {
        logger->error("登录失败: code={}, msg={}", result.code, result.msg);
        return result.code;
    }

    authorization = result.data["authorization"].get<std::string>();
    logger->info("登录成功: user={}, token={:.20}...", user_name, authorization);
    save_file();
    return 200;
}

void AuthService::save_file() {
    auto logger = get_logger("auth");
    try {
        json account_info = {
            {"userName", user_name},
            {"passWord", password},
            {"authorization", authorization},
            {"deviceType", devicetype},
            {"osVersion", osversion},
            {"loginuuid", loginuuid},
        };

        // Encrypt password before saving
        account_info = encrypt_account_passwords(account_info);
        ConfigManager::save_account(user_name, account_info);
        logger->info("账号已保存");
    } catch (const std::exception& e) {
        logger->error("保存账号失败: {}", e.what());
    }
}

void AuthService::read_ini(const std::string& user_name,
                            const std::string& password,
                            const std::string& authorization) {
    auto logger = get_logger("auth");
    try {
        json account;
        if (!user_name.empty()) {
            account = ConfigManager::get_account(user_name);
        }
        if (account.empty()) {
            account = ConfigManager::get_account();
        }

        std::string u = user_name;
        std::string p = password;
        std::string a = authorization;

        if (!account.empty()) {
            if (u.empty()) u = account.value("userName", u);
            if (p.empty()) p = account.value("passWord", p);
            if (a.empty()) a = account.value("authorization", a);

            // Decrypt password if encrypted
            if (!p.empty() && p.starts_with("enc:")) {
                p = decrypt_credential(p);
            }

            std::string dt = account.value("deviceType", "");
            std::string ov = account.value("osVersion", "");
            std::string lu = account.value("loginuuid", "");

            if (!dt.empty()) devicetype = dt;
            if (!ov.empty()) osversion = ov;
            if (!lu.empty()) loginuuid = lu;
        } else {
            // Try old format
            auto config = ConfigManager::load_config();
            std::string dt = config.value("deviceType", "");
            std::string ov = config.value("osVersion", "");
            std::string lu = config.value("loginuuid", "");

            if (!dt.empty()) devicetype = dt;
            if (!ov.empty()) osversion = ov;
            if (!lu.empty()) loginuuid = lu;

            if (u.empty()) u = config.value("userName", "");
            if (p.empty()) p = config.value("passWord", "");
            if (a.empty()) a = config.value("authorization", "");
        }

        this->user_name = u;
        this->password = p;
        this->authorization = a;
    } catch (const std::exception& e) {
        logger->error("获取配置失败: {}", e.what());
        throw std::runtime_error("无法从配置获取账号信息");
    }
}

}  // namespace app
