#pragma once

#include <atomic>
#include <chrono>
#include <mutex>

namespace app {

/// 基于令牌桶算法的速度限制器，线程安全。
/// 以 KB/s 为单位控制传输速率。
/// 当 limit=0 时表示不限制速度。
class SpeedLimiter {
public:
    /// 初始化限速器。
    /// @param limit_kbps 速度限制（KB/s），0 表示不限制。
    explicit SpeedLimiter(int limit_kbps = 0);

    /// 获取当前速度限制（KB/s）。
    [[nodiscard]] int limit_kbps() const noexcept { return _limit_kbps.load(); }

    /// 动态调整速度限制。
    /// @param limit_kbps 新的速度限制（KB/s），0 表示不限制。
    void set_limit(int limit_kbps);

    /// 消费指定字节数，返回需等待的秒数。
    /// 当不限制速度（limit=0）时，立即返回 0。
    /// @param bytes_count 要消费的字节数。
    /// @return 需要等待的秒数（可能为 0 或小数）。
    double consume(int64_t bytes_count);

private:
    void refill();
    void update_max_tokens();

    std::atomic<int> _limit_kbps;
    double _max_tokens{0.0};
    double _tokens{0.0};
    std::chrono::steady_clock::time_point _last_refill;
    std::mutex _mutex;
};

}  // namespace app
