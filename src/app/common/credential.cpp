#include "app/common/credential.hpp"
#include "app/common/config.hpp"
#include "app/common/log.hpp"

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include <filesystem>
#include <fstream>
#include <vector>
#include <random>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#endif

namespace app {

namespace fs = std::filesystem;
using json = nlohmann::json;

// ============================================================
// Constants (matching Python PBKDF2 parameters)
// ============================================================
static constexpr int SALT_SIZE = 16;
static constexpr int ITERATIONS = 600'000;
static constexpr int KEY_LENGTH = 32;  // AES-256
static constexpr int NONCE_SIZE = 12;  // GCM nonce

// Key file path
static fs::path get_key_file_path() {
    return get_config_dir() / ".keyfile";
}

// ============================================================
// Machine ID
// ============================================================
static std::string get_machine_id() {
    std::string machine_id;
#ifdef _WIN32
    char computer_name[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = sizeof(computer_name);
    if (GetComputerNameA(computer_name, &size)) {
        machine_id += computer_name;
    } else {
        machine_id += "unknown";
    }
#else
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        machine_id += hostname;
    } else {
        machine_id += "unknown";
    }
#endif

    machine_id += "|" + std::string(
#ifdef __x86_64__
        "x86_64"
#elif defined(__aarch64__)
        "aarch64"
#elif defined(__i386__)
        "i386"
#else
        "unknown"
#endif
    );

    const char* home = std::getenv("HOME");
    if (!home) home = std::getenv("USERPROFILE");
    machine_id += "|" + std::string(home ? home : ".");

    return machine_id;
}

// ============================================================
// PBKDF2 key derivation using OpenSSL
// ============================================================
static std::vector<uint8_t> derive_key(
    const std::vector<uint8_t>& salt,
    const std::string& machine_id
) {
    std::vector<uint8_t> key(KEY_LENGTH);
    PKCS5_PBKDF2_HMAC(
        machine_id.c_str(),
        static_cast<int>(machine_id.size()),
        salt.data(),
        static_cast<int>(salt.size()),
        ITERATIONS,
        EVP_sha256(),
        KEY_LENGTH,
        key.data()
    );
    return key;
}

// ============================================================
// AES-256-GCM encrypt/decrypt helpers
// ============================================================
static std::vector<uint8_t> aes_gcm_encrypt(
    const std::vector<uint8_t>& key,
    const std::vector<uint8_t>& plaintext
) {
    std::vector<uint8_t> nonce(NONCE_SIZE);
    RAND_bytes(nonce.data(), NONCE_SIZE);

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) throw std::runtime_error("EVP_CIPHER_CTX_new failed");

    std::vector<uint8_t> ciphertext(plaintext.size() + 16);
    int out_len = 0;
    int total_len = 0;

    EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, key.data(), nonce.data());

    EVP_EncryptUpdate(ctx, ciphertext.data(), &out_len, plaintext.data(),
                      static_cast<int>(plaintext.size()));
    total_len = out_len;

    EVP_EncryptFinal_ex(ctx, ciphertext.data() + total_len, &out_len);
    total_len += out_len;

    // Get GCM tag
    std::vector<uint8_t> tag(16);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag.data());

    EVP_CIPHER_CTX_free(ctx);

    // Result: nonce + ciphertext + tag
    std::vector<uint8_t> result;
    result.insert(result.end(), nonce.begin(), nonce.end());
    result.insert(result.end(), ciphertext.begin(), ciphertext.begin() + total_len);
    result.insert(result.end(), tag.begin(), tag.end());
    return result;
}

static std::vector<uint8_t> aes_gcm_decrypt(
    const std::vector<uint8_t>& key,
    const std::vector<uint8_t>& encrypted_data
) {
    if (encrypted_data.size() < NONCE_SIZE + 16) {
        throw std::runtime_error("Invalid encrypted data");
    }

    auto nonce_begin = encrypted_data.begin();
    auto nonce_end = nonce_begin + NONCE_SIZE;
    auto ct_begin = nonce_end;
    auto ct_end = encrypted_data.end() - 16;  // Last 16 bytes are tag
    auto tag_begin = ct_end;

    std::vector<uint8_t> nonce(nonce_begin, nonce_end);
    std::vector<uint8_t> ciphertext(ct_begin, ct_end);
    std::vector<uint8_t> tag(tag_begin, encrypted_data.end());

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) throw std::runtime_error("EVP_CIPHER_CTX_new failed");

    std::vector<uint8_t> plaintext(ciphertext.size());

    EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, key.data(), nonce.data());

    int out_len = 0;
    int total_len = 0;
    EVP_DecryptUpdate(ctx, plaintext.data(), &out_len, ciphertext.data(),
                      static_cast<int>(ciphertext.size()));
    total_len = out_len;

    // Set expected tag
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16,
                        const_cast<uint8_t*>(tag.data()));

    int ret = EVP_DecryptFinal_ex(ctx, plaintext.data() + out_len, &out_len);
    EVP_CIPHER_CTX_free(ctx);

    if (ret <= 0) {
        throw std::runtime_error("AES-GCM decryption failed (authentication error)");
    }

    total_len += out_len;
    plaintext.resize(total_len);
    return plaintext;
}

// ============================================================
// Base64 encode/decode
// ============================================================
static const char BASE64_CHARS[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::string base64_encode(const std::vector<uint8_t>& data) {
    std::string result;
    result.reserve(((data.size() + 2) / 3) * 4);
    size_t i = 0;
    while (i < data.size()) {
        uint32_t octet_a = (i < data.size()) ? data[i++] : 0;
        uint32_t octet_b = (i < data.size()) ? data[i++] : 0;
        uint32_t octet_c = (i < data.size()) ? data[i++] : 0;
        uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;

        result.push_back(BASE64_CHARS[(triple >> 18) & 0x3F]);
        result.push_back(BASE64_CHARS[(triple >> 12) & 0x3F]);
        result.push_back(BASE64_CHARS[(triple >> 6) & 0x3F]);
        result.push_back(BASE64_CHARS[triple & 0x3F]);
    }

    // Add padding
    size_t mod = data.size() % 3;
    if (mod > 0) {
        result[result.size() - 1] = '=';
        if (mod == 1) {
            result[result.size() - 2] = '=';
        }
    }

    return result;
}

static std::vector<uint8_t> base64_decode(const std::string& input) {
    std::vector<uint8_t> result;
    result.reserve(input.size() * 3 / 4);

    auto decode_char = [](char c) -> uint32_t {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return 64;  // Invalid
    };

    for (size_t i = 0; i < input.size(); i += 4) {
        uint32_t a = decode_char(input[i]);
        uint32_t b = (i + 1 < input.size()) ? decode_char(input[i + 1]) : 64;
        uint32_t c = (i + 2 < input.size()) ? decode_char(input[i + 2]) : 64;
        uint32_t d = (i + 3 < input.size()) ? decode_char(input[i + 3]) : 64;

        if (a == 64 || b == 64) break;

        uint32_t triple = (a << 18) + (b << 12) + (c << 6) + d;
        result.push_back((triple >> 16) & 0xFF);

        if (c != 64) {
            result.push_back((triple >> 8) & 0xFF);
        }
        if (d != 64) {
            result.push_back(triple & 0xFF);
        }
    }

    return result;
}

// ============================================================
// Key management
// ============================================================
static std::vector<uint8_t> load_or_create_key() {
    auto logger = get_logger("credential");
    std::string machine_id = get_machine_id();
    std::vector<uint8_t> salt(SALT_SIZE);
    RAND_bytes(salt.data(), SALT_SIZE);
    std::vector<uint8_t> derived_key = derive_key(salt, machine_id);

    fs::path key_file = get_key_file_path();

    if (fs::exists(key_file)) {
        try {
            std::ifstream f(key_file, std::ios::binary);
            if (!f) throw std::runtime_error("Cannot open key file");

            std::vector<uint8_t> stored_salt(SALT_SIZE);
            f.read(reinterpret_cast<char*>(stored_salt.data()), SALT_SIZE);
            if (!f) throw std::runtime_error("Failed to read salt");

            f.seekg(0, std::ios::end);
            auto remaining = static_cast<size_t>(f.tellg()) - SALT_SIZE;
            f.seekg(SALT_SIZE);
            std::vector<uint8_t> encrypted_key(remaining);
            f.read(reinterpret_cast<char*>(encrypted_key.data()), remaining);

            std::vector<uint8_t> dk = derive_key(stored_salt, machine_id);
            return aes_gcm_decrypt(dk, encrypted_key);
        } catch (const std::exception& e) {
            logger->warn("密钥文件损坏，重新生成: {}", e.what());
        }
    }

    // Generate new key and store encrypted
    std::vector<uint8_t> random_key(KEY_LENGTH);
    RAND_bytes(random_key.data(), KEY_LENGTH);

    std::vector<uint8_t> encrypted_key = aes_gcm_encrypt(derived_key, random_key);

    fs::create_directories(get_config_dir());
    {
        std::ofstream f(key_file, std::ios::binary);
        if (!f) throw std::runtime_error("Cannot create key file");
        f.write(reinterpret_cast<const char*>(salt.data()), salt.size());
        f.write(reinterpret_cast<const char*>(encrypted_key.data()), encrypted_key.size());
    }

    // Set file permissions to 600 (owner read/write only)
#ifdef _WIN32
    // On Windows, we can't easily set 600; skip for now
#else
    chmod(key_file.c_str(), S_IRUSR | S_IWUSR);
#endif

    return random_key;
}

// ============================================================
// Public API
// ============================================================
std::string encrypt_credential(const std::string& plaintext) {
    if (plaintext.empty()) return "";

    auto logger = get_logger("credential");
    try {
        std::vector<uint8_t> key = load_or_create_key();
        std::vector<uint8_t> plain_bytes(plaintext.begin(), plaintext.end());
        std::vector<uint8_t> encrypted = aes_gcm_encrypt(key, plain_bytes);
        return "enc:" + base64_encode(encrypted);
    } catch (const std::exception& e) {
        logger->error("加密凭据失败: {}", e.what());
        return plaintext;
    }
}

std::string decrypt_credential(const std::string& ciphertext) {
    if (ciphertext.empty() || !ciphertext.starts_with("enc:")) {
        return ciphertext;
    }

    auto logger = get_logger("credential");
    try {
        std::vector<uint8_t> key = load_or_create_key();
        std::string b64_part = ciphertext.substr(4);
        std::vector<uint8_t> encrypted = base64_decode(b64_part);
        std::vector<uint8_t> decrypted = aes_gcm_decrypt(key, encrypted);
        return std::string(decrypted.begin(), decrypted.end());
    } catch (const std::exception& e) {
        logger->error("解密凭据失败: {}", e.what());
        return ciphertext;
    }
}

json encrypt_account_passwords(const json& account_info) {
    if (account_info.is_null()) return account_info;
    if (!account_info.is_object()) return account_info;

    json result = account_info;
    if (result.contains("passWord")) {
        std::string pwd = result["passWord"].get<std::string>();
        if (!pwd.empty() && !pwd.starts_with("enc:")) {
            result["passWord"] = encrypt_credential(pwd);
        }
    }
    return result;
}

json decrypt_account_passwords(const json& account_info) {
    if (account_info.is_null()) return account_info;
    if (!account_info.is_object()) return account_info;

    json result = account_info;
    if (result.contains("passWord")) {
        std::string pwd = result["passWord"].get<std::string>();
        if (!pwd.empty() && pwd.starts_with("enc:")) {
            result["passWord"] = decrypt_credential(pwd);
        }
    }
    return result;
}

}  // namespace app
