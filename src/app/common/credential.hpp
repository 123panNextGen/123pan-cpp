#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace app {

/// 加密凭据字符串，返回 base64 编码的密文（前缀 "enc:"）。
/// @param plaintext 明文凭据。
/// @return "enc:" + base64 密文，空字符串原样返回。
std::string encrypt_credential(const std::string& plaintext);

/// 解密凭据字符串。
/// @param ciphertext "enc:" 前缀的 base64 密文，或明文。
/// @return 解密后的明文。非加密格式原样返回。
std::string decrypt_credential(const std::string& ciphertext);

/// 加密账户信息中的密码字段。
nlohmann::json encrypt_account_passwords(const nlohmann::json& account_info);

/// 解密账户信息中的密码字段。
nlohmann::json decrypt_account_passwords(const nlohmann::json& account_info);

}  // namespace app
