#include <gtest/gtest.h>
#include "app/common/config.hpp"

#include <fstream>
#include <filesystem>

using namespace app;
namespace fs = std::filesystem;

class ConfigManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        _temp_dir = fs::temp_directory_path() / ("123pan_test_" + std::to_string(std::rand()));
        fs::create_directories(_temp_dir);
        _old_config_dir = get_config_dir();
        _old_config_file = get_config_file_path();
    }

    void TearDown() override {
        fs::remove_all(_temp_dir);
    }

    fs::path _temp_dir;
    fs::path _old_config_dir;
    fs::path _old_config_file;
};

TEST(ConfigTest, LoadDefaultConfig) {
    // This test relies on the actual config directory
    auto config = ConfigManager::load_config();
    EXPECT_EQ(config["currentAccount"].get<std::string>(), "");
    EXPECT_TRUE(config["accounts"].is_object());
    EXPECT_TRUE(config["settings"].contains("downloadSpeedLimit"));
    EXPECT_EQ(config["settings"]["downloadSpeedLimit"].get<int>(), 0);
    EXPECT_TRUE(config["settings"]["multiThreadDownload"].get<bool>());
}

TEST(ConfigTest, SaveAndGetSetting) {
    ConfigManager::set_setting("downloadSpeedLimit", 500);
    EXPECT_EQ(ConfigManager::get_setting<int>("downloadSpeedLimit", 0), 500);

    // Reset
    ConfigManager::set_setting("downloadSpeedLimit", 0);
    EXPECT_EQ(ConfigManager::get_setting<int>("downloadSpeedLimit", 0), 0);
}

TEST(ConfigTest, GetSettingWithFallback) {
    EXPECT_EQ(ConfigManager::get_setting<int>("nonexistent_key", 42), 42);
    EXPECT_EQ(ConfigManager::get_setting<std::string>("nonexistent_key", "fallback"), "fallback");
    EXPECT_EQ(ConfigManager::get_setting<bool>("nonexistent_key", true), true);
}

TEST(ConfigTest, AccountLifecycle) {
    // Save account
    json account_info = {
        {"userName", "test_user_cpp"},
        {"passWord", "my_password"},
        {"authorization", "tok_xxx"},
    };
    EXPECT_TRUE(ConfigManager::save_account("test_user_cpp", account_info));

    // Verify account name appears
    auto names = ConfigManager::get_account_names();
    EXPECT_NE(std::find(names.begin(), names.end(), "test_user_cpp"), names.end());

    // Verify current account is set
    EXPECT_EQ(ConfigManager::get_current_account_name(), "test_user_cpp");

    // Get account
    auto account = ConfigManager::get_account("test_user_cpp");
    EXPECT_EQ(account["userName"].get<std::string>(), "test_user_cpp");

    // Save second account and switch
    json account2 = {{"userName", "user2"}, {"passWord", "pwd2"}};
    ConfigManager::save_account("user2", account2);
    ConfigManager::set_current_account("user2");
    EXPECT_EQ(ConfigManager::get_current_account_name(), "user2");
}

TEST(ConfigTest, SetCurrentAccountNonexistent) {
    EXPECT_FALSE(ConfigManager::set_current_account("nonexistent_account_xyz"));
}
