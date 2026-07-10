#include <gtest/gtest.h>
#include "app/common/speed_limiter.hpp"

#include <thread>
#include <vector>
#include <chrono>

using namespace app;
using namespace std::chrono_literals;

TEST(SpeedLimiterTest, ConsumeUnlimited) {
    SpeedLimiter limiter(0);
    EXPECT_DOUBLE_EQ(limiter.consume(1024 * 1024), 0.0);
    EXPECT_DOUBLE_EQ(limiter.consume(0), 0.0);
}

TEST(SpeedLimiterTest, ConsumeWithinLimit) {
    SpeedLimiter limiter(100);
    // Fill tokens (manually set for testing)
    limiter.set_limit(100);
    // Consume 50KB - should be within limits if tokens are full
    double wait = limiter.consume(50 * 1024);
    // Initial tokens = 200 (2 seconds worth @ 100KB/s),
    // so consuming 50KB should succeed immediately
    EXPECT_DOUBLE_EQ(wait, 0.0);
}

TEST(SpeedLimiterTest, ConsumeExceedsLimitReturnsWait) {
    SpeedLimiter limiter(100);
    // First consume all tokens
    limiter.consume(200 * 1024);  // consume the initial 200KB

    // Now consume 50KB - should need to wait
    double wait = limiter.consume(50 * 1024);
    EXPECT_GT(wait, 0.0);
    // wait = deficit / limit = 50 / 100 = 0.5
    EXPECT_NEAR(wait, 0.5, 0.01);
}

TEST(SpeedLimiterTest, SetLimitDuringOperation) {
    SpeedLimiter limiter(100);
    limiter.consume(200 * 1024);  // empty tokens

    limiter.set_limit(50);
    // Max tokens should now be 100 (50*2)
    EXPECT_EQ(limiter.limit_kbps(), 50);
}

TEST(SpeedLimiterTest, SetLimitZeroDisables) {
    SpeedLimiter limiter(100);
    limiter.set_limit(0);
    EXPECT_EQ(limiter.limit_kbps(), 0);
    EXPECT_DOUBLE_EQ(limiter.consume(999999999), 0.0);
}

TEST(SpeedLimiterTest, ThreadSafety) {
    SpeedLimiter limiter(500);
    std::atomic<int> errors{0};

    auto worker = [&]() {
        for (int i = 0; i < 100; ++i) {
            try {
                limiter.consume(64 * 1024);
            } catch (...) {
                ++errors;
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back(worker);
    }
    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(errors.load(), 0);
}

TEST(SpeedLimiterTest, LimitKbpsProperty) {
    SpeedLimiter limiter(200);
    EXPECT_EQ(limiter.limit_kbps(), 200);
    limiter.set_limit(300);
    EXPECT_EQ(limiter.limit_kbps(), 300);
}
