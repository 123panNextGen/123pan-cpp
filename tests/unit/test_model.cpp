#include <gtest/gtest.h>
#include "app/api/model.hpp"

using namespace app;

TEST(ModelTest, FileItemModelFromDictPascalCase) {
    json data = {
        {"FileId", 1}, {"FileName", "test.txt"}, {"Type", 0},
        {"Size", 1024}, {"CreateAt", 1700000000}, {"UpdateAt", 1700000001},
        {"Hidden", false}, {"Etag", "abc"}, {"S3KeyFlag", ""},
        {"ContentType", "text/plain"}, {"ParentFileId", 0},
        {"PinYin", "test"}, {"StarredStatus", false},
    };

    auto item = FileItemModel::from_json(data);
    EXPECT_EQ(item.file_id, 1);
    EXPECT_EQ(item.file_name, "test.txt");
    EXPECT_EQ(item.type, 0);
    EXPECT_EQ(item.size, 1024);
    EXPECT_FALSE(item.is_dir());
}

TEST(ModelTest, FileItemModelFromDictCamelCase) {
    json data = {
        {"fileId", 2}, {"fileName", "folder"}, {"type", 1},
        {"size", 0}, {"createAt", 1700000000}, {"updateAt", 1700000001},
        {"hidden", false}, {"etag", "def"}, {"s3keyFlag", ""},
        {"contentType", ""}, {"parentFileId", 0}, {"pinYin", ""},
        {"starredStatus", false},
    };

    auto item = FileItemModel::from_json(data);
    EXPECT_EQ(item.file_id, 2);
    EXPECT_EQ(item.file_name, "folder");
    EXPECT_EQ(item.type, 1);
    EXPECT_TRUE(item.is_dir());
}

TEST(ModelTest, FileItemModelFromDictMissingFieldsUseDefaults) {
    json data = {};
    auto item = FileItemModel::from_json(data);
    EXPECT_EQ(item.file_id, 0);
    EXPECT_EQ(item.file_name, "");
    EXPECT_EQ(item.type, 0);
    EXPECT_EQ(item.size, 0);
}

TEST(ModelTest, FileItemModelToJsonRoundtrip) {
    json data = {
        {"FileId", 1}, {"FileName", "test.txt"}, {"Type", 0},
        {"Size", 1024}, {"CreateAt", 1700000000}, {"UpdateAt", 1700000001},
        {"Hidden", false}, {"Etag", "abc"}, {"S3KeyFlag", ""},
        {"ContentType", "text/plain"}, {"ParentFileId", 0},
        {"PinYin", "test"}, {"StarredStatus", false},
    };

    auto item = FileItemModel::from_json(data);
    auto out = item.to_json();
    EXPECT_EQ(out["FileId"].get<int64_t>(), 1);
    EXPECT_EQ(out["FileName"].get<std::string>(), "test.txt");
    EXPECT_EQ(out["Type"].get<int>(), 0);
    EXPECT_EQ(out["Size"].get<int64_t>(), 1024);
}

TEST(ModelTest, FileListDataFromDict) {
    json data = {
        {"Next", "-1"}, {"Len", 10}, {"Total", 100}, {"IsFirst", true},
        {"InfoList", json::array({
            {{"FileId", 1}, {"FileName", "a.txt"}, {"Type", 0}, {"Size", 100},
             {"CreateAt", 1700000000}, {"UpdateAt", 1700000000},
             {"Hidden", false}, {"Etag", ""}, {"S3KeyFlag", ""},
             {"ContentType", ""}, {"ParentFileId", 0}, {"PinYin", ""},
             {"StarredStatus", false}},
        })},
    };

    auto fld = FileListData::from_json(data);
    EXPECT_EQ(fld.next, "-1");
    EXPECT_EQ(fld.len, 10);
    EXPECT_EQ(fld.total, 100);
    EXPECT_TRUE(fld.is_first);
    EXPECT_EQ(fld.info_list.size(), 1);
    EXPECT_EQ(fld.info_list[0].file_name, "a.txt");
}
