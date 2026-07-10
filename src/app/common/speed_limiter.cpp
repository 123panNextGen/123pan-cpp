#include "app/common/speed_limiter.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace app {

SpeedLimiter::SpeedLimiter(int limit_kbps)
    : _limit_kbps(limit_kbps)
    , _last_refill(std::chrono::steady_clock::now())
{
    update_max_tokens();
    if (limit_kbps > 0) {
        _tokens = _max_tokens;
    } else {
        _tokens = std::numeric_limits<double>::infinity();
    }
}

void SpeedLimiter::set_limit(int limit_kbps) {
    std::lock_guard<std::mutex> lock(_mutex);
    _limit_kbps.store(std::max(0, limit_kbps));
    update_max_tokens();
    // Cap existing tokens to new max
    _tokens = std::min(_tokens, _max_tokens);
}

void SpeedLimiter::update_max_tokens() {
    int limit = _limit_kbps.load();
    if (limit > 0) {
        _max_tokens = static_cast<double>(limit) * 2.0;  // 2 seconds worth
    } else {
        _max_tokens = std::numeric_limits<double>::infinity();
    }
}

void SpeedLimiter::refill() {
    auto now = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(now - _last_refill).count();
    _last_refill = now;

    int limit = _limit_kbps.load();
    if (limit > 0 && elapsed > 0) {
        _tokens += elapsed * static_cast<double>(limit);
        if (_tokens > _max_tokens) {
            _tokens = _max_tokens;
        }
    }
}

double SpeedLimiter::consume(int64_t bytes_count) {
    int limit = _limit_kbps.load();
    if (limit <= 0) {
        return 0.0;
    }

    double kb = static_cast<double>(bytes_count) / 1024.0;

    std::lock_guard<std::mutex> lock(_mutex);
    refill();

    if (_tokens >= kb) {
        _tokens -= kb;
        return 0.0;
    } else {
        double deficit = kb - _tokens;
        double wait = deficit / static_cast<double>(limit);
        _tokens = 0.0;
        return wait;
    }
}

}  // namespace app
