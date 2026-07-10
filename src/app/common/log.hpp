#pragma once

#include <spdlog/spdlog.h>
#include <memory>
#include <string>
#include <vector>

namespace app {

/// 初始化日志系统。
/// 创建日志目录，设置文件和控制台输出，清理旧日志。
/// 应在程序启动时调用一次。
void init_logging(const std::string& config_dir);

/// 获取指定名称的 logger。
std::shared_ptr<spdlog::logger> get_logger(const std::string& name = "123pan");

/// 动态设置全局日志等级。
/// level_name: "DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"
void set_log_level(const std::string& level_name);

/// 获取当前日志等级名称。
std::string get_current_level_name();

/// 获取所有可用的日志等级名称列表。
std::vector<std::string> get_level_names();

/// 在系统默认应用中打开日志文件。
void open_log_file();

}  // namespace app
