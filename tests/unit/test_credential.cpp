#include <gtest/gtest.h>
#include "app/common/credential.hpp"

using namespace app;

TEST(CredentialTest, EncryptDecryptRoundtrip) {
    std::string original = "MySecretPassword123!";
    std::string encrypted = encrypt_credential(original);
    EXPECT_TRUE(encrypted.starts_with("enc:"));
    std::string decrypted = decrypt_credential(encrypted);
    EXPECT_EQ(decrypted, original);
}

TEST(CredentialTest, DecryptPlaintext) {
    EXPECT_EQ(decrypt_credential(""), "");
    EXPECT_EQ(decrypt_credential("hello"), "hello");
    EXPECT_EQ(decrypt_credential("enc:"), "enc:");
    EXPECT_EQ(decrypt_credential("enc:invalid"), "enc:invalid");
}

TEST(CredentialTest, EncryptEmptyString) {
    EXPECT_EQ(encrypt_credential(""), "");
}

TEST(CredentialTest, EncryptMultipleCallsDifferentResults) {
    std::string plain = "same password";
    std::string e1 = encrypt_credential(plain);
    std::string e2 = encrypt_credential(plain);
    EXPECT_NE(e1, e2);  // Different due to GCM nonce randomness
    EXPECT_EQ(decrypt_credential(e1), plain);
    EXPECT_EQ(decrypt_credential(e2), plain);
}

TEST(CredentialTest, AccountPasswordsRoundtrip) {
    nlohmann::json info = {
        {"userName", "test"},
        {"passWord", "p@ss123"},
    };
    auto encrypted = encrypt_account_passwords(info);
    EXPECT_TRUE(encrypted["passWord"].get<std::string>().starts_with("enc:"));
    EXPECT_EQ(encrypted["userName"].get<std::string>(), "test");

    auto decrypted = decrypt_account_passwords(encrypted);
    EXPECT_EQ(decrypted["passWord"].get<std::string>(), "p@ss123");
}

TEST(CredentialTest, AccountPasswordsSkipNonEncrypted) {
    nlohmann::json info = {
        {"userName", "test"},
        {"passWord", "plain_pwd"},
    };
    auto result = decrypt_account_passwords(info);
    EXPECT_EQ(result["passWord"].get<std::string>(), "plain_pwd");
}

TEST(CredentialTest, AccountPasswordsEmpty) {
    EXPECT_EQ(encrypt_account_passwords(nlohmann::json::object()), nlohmann::json::object());
    EXPECT_EQ(decrypt_account_passwords(nlohmann::json::object()), nlohmann::json::object());
}
