#pragma once

#include <memory>
#include <string>

namespace app {

class NetSession;

/// 认证与凭证管理服务。
class AuthService {
public:
    explicit AuthService(std::shared_ptr<NetSession> session);

    /// 登录123云盘并获取授权令牌。返回 HTTP 状态码（200=成功）。
    int login();

    /// 从配置文件读取账号信息。
    /// @param user_name 用户名
    /// @param password 密码
    /// @param authorization 授权令牌
    void read_ini(const std::string& user_name = "",
                  const std::string& password = "",
                  const std::string& authorization = "");

    /// 将账户信息保存到配置文件。
    void save_file();

    /// 将设备/用户信息同步到 NetSession。
    void sync_to_session();

    // 公共属性
    std::string devicetype;
    std::string osversion;
    std::string loginuuid;
    std::string user_name;
    std::string password;
    std::string authorization;

private:
    std::shared_ptr<NetSession> _session;
};

}  // namespace app
