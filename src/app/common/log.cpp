#include "app/common/log.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <filesystem>
#include <chrono>
#include <format>
#include <cstdlib>
#include <unordered_map>

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <cstdlib>
#else
#include <cstdlib>
#endif

namespace app {

namespace fs = std::filesystem;

// ============================================================
// Internal state
// ============================================================
static std::string g_log_file_path;
static spdlog::level::level_enum g_current_level = spdlog::level::debug;

static const std::unordered_map<std::string, spdlog::level::level_enum> LEVEL_MAP = {
    {"DEBUG",    spdlog::level::debug},
    {"INFO",     spdlog::level::info},
    {"WARNING",  spdlog::level::warn},
    {"ERROR",    spdlog::level::err},
    {"CRITICAL", spdlog::level::critical},
};

static const std::vector<std::string> LEVEL_NAMES = {
    "DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"
};

// ============================================================
// Helper: clean up old log files
// ============================================================
static void cleanup_old_logs(const fs::path& log_dir, int retention_days) {
    if (!fs::exists(log_dir)) return;

    auto cutoff = std::chrono::system_clock::now() - std::chrono::hours(24 * retention_days);

    for (const auto& entry : fs::directory_iterator(log_dir)) {
        if (!entry.is_regular_file()) continue;
        auto filename = entry.path().filename().string();
        if (!filename.starts_with("log_") || !filename.ends_with(".log")) continue;

        try {
            auto ftime = fs::last_write_time(entry.path());
            auto sctp = std::chrono::clock_cast<std::chrono::system_clock>(
                decltype(ftime)::clock::now()
            );
            // Use last_write_time for comparison
            auto file_time = fs::last_write_time(entry.path());
            // Simple approach: if too old based on filename timestamp
            auto stem = entry.path().stem().string();  // "log_2024-07-11_12-00-00"
            if (stem.size() > 4) {
                auto ts_str = stem.substr(4);  // Remove "log_" prefix
                // Parse as YYYY-MM-DD_HH-MM-SS
                // We use a simple heuristic: count the number of dashes
                // and check if it looks like a timestamp
                if (std::count(ts_str.begin(), ts_str.end(), '-') >= 3) {
                    // This looks like a timestamp, check age
                    // For simplicity, we use file system time
                    (void)cutoff;
                    // Just delete files older than retention_days
                    auto now = std::chrono::system_clock::now();
                    auto last_write = std::chrono::system_clock::now();
                    // Actually use filesystem last write time
                    auto ft = fs::last_write_time(entry.path());
                    auto sctp2 = decltype(ft)::clock::to_sys(ft);
                    if (sctp2 < cutoff) {
                        fs::remove(entry.path());
                    }
                }
            }
        } catch (...) {
            // Ignore errors cleaning up
        }
    }
}

// ============================================================
// Public API
// ============================================================
void init_logging(const std::string& config_dir) {
    fs::path config_path(config_dir);
    fs::path log_dir = config_path / "logs";

    // Create log directory
    if (!fs::exists(log_dir)) {
        fs::create_directories(log_dir);
    }

    // Generate log filename with timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now{};
#ifdef _WIN32
    localtime_s(&tm_now, &time_t_now);
#else
    localtime_r(&time_t_now, &tm_now);
#endif

    char ts_buf[64];
    std::strftime(ts_buf, sizeof(ts_buf), "%Y-%m-%d_%H-%M-%S", &tm_now);
    std::string log_filename = std::format("log_{}.log", ts_buf);
    fs::path log_file = log_dir / log_filename;
    g_log_file_path = log_file.string();

    // Clean up old logs (7 days)
    cleanup_old_logs(log_dir, 7);

    // Configure spdlog sinks
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(g_current_level);

    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
        g_log_file_path, true
    );
    file_sink->set_level(g_current_level);

    // Create root logger
    auto logger = std::make_shared<spdlog::logger>(
        "123pan",
        spdlog::sinks_init_list{console_sink, file_sink}
    );
    logger->set_level(g_current_level);
    logger->set_pattern("%Y-%m-%d %H:%M:%S.%e - %n - %l - %v");
    logger->flush_on(spdlog::level::info);

    spdlog::register_logger(logger);
    spdlog::set_default_logger(logger);
}

std::shared_ptr<spdlog::logger> get_logger(const std::string& name) {
    auto logger = spdlog::get(name);
    if (!logger) {
        logger = spdlog::default_logger()->clone(name);
        spdlog::register_logger(logger);
    }
    return logger;
}

void set_log_level(const std::string& level_name) {
    auto it = LEVEL_MAP.find(level_name);
    if (it != LEVEL_MAP.end()) {
        g_current_level = it->second;
    } else {
        g_current_level = spdlog::level::debug;
    }

    // Update all registered loggers
    spdlog::apply_all([&](std::shared_ptr<spdlog::logger> l) {
        l->set_level(g_current_level);
    });
}

std::string get_current_level_name() {
    for (const auto& [name, level] : LEVEL_MAP) {
        if (level == g_current_level) return name;
    }
    return "DEBUG";
}

std::vector<std::string> get_level_names() {
    return LEVEL_NAMES;
}

void open_log_file() {
    if (g_log_file_path.empty()) return;

#ifdef _WIN32
    ShellExecuteA(nullptr, "open", g_log_file_path.c_str(), nullptr, nullptr, SW_SHOW);
#elif defined(__APPLE__)
    std::system(std::format("open \"{}\"", g_log_file_path).c_str());
#else
    std::system(std::format("xdg-open \"{}\"", g_log_file_path).c_str());
#endif
}

}  // namespace app
