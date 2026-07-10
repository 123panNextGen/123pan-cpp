#include <gtest/gtest.h>
#include "app/common/api.hpp"

using namespace app;

TEST(ApiUtilsTest, FormatFileSizeBytes) {
    EXPECT_EQ(format_file_size(0), "0 B");
    EXPECT_EQ(format_file_size(512), "512 B");
    EXPECT_EQ(format_file_size(1023), "1023 B");
}

TEST(ApiUtilsTest, FormatFileSizeKB) {
    EXPECT_EQ(format_file_size(1024), "1.00 KB");
    EXPECT_EQ(format_file_size(2048), "2.00 KB");
    EXPECT_EQ(format_file_size(1536), "1.50 KB");
}

TEST(ApiUtilsTest, FormatFileSizeMB) {
    EXPECT_EQ(format_file_size(1048576), "1.00 MB");
    EXPECT_EQ(format_file_size(1572864), "1.50 MB");
}

TEST(ApiUtilsTest, FormatFileSizeGB) {
    auto result = format_file_size(1073741824);
    EXPECT_TRUE(result.find("GB") != std::string::npos);
    EXPECT_TRUE(result.starts_with("1.0"));
}

TEST(ApiUtilsTest, FormatFileSizeTB) {
    auto result = format_file_size(1099511627776);
    EXPECT_TRUE(result.find("TB") != std::string::npos);
}

TEST(ApiUtilsTest, GetFileTypeName) {
    EXPECT_EQ(get_file_type_name(1), "文件夹");
    EXPECT_EQ(get_file_type_name(0), "文件");
    EXPECT_EQ(get_file_type_name(2), "文件");
}

TEST(ApiUtilsTest, GetFileExtension) {
    EXPECT_EQ(get_file_extension("test.txt"), ".txt");
    EXPECT_EQ(get_file_extension("no_ext"), "");
    EXPECT_EQ(get_file_extension(""), "");
}
