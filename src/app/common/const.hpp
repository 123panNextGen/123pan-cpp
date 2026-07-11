#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace app {

/// 应用版本信息
constexpr int YEAR = 2026;
constexpr const char* VERSION = "1.0.0";
constexpr const char* ABOUT_URL = "https://github.com/123panNextGen/123pan-cpp";

/// 日志保留天数
constexpr int LOG_RETENTION_DAYS = 7;

/// 云盘最大容量（字节）默认 2TB
constexpr int64_t MAX_STORAGE_CAPACITY = 2LL * 1024 * 1024 * 1024 * 1024;  // 2TB

/// 设备伪装数据（声明，定义在 devices.cpp 中）
extern const std::vector<std::string> ALL_DEVICE_TYPES;
extern const std::vector<std::string> ALL_OS_VERSIONS;

}  // namespace app
