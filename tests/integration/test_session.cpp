#include <gtest/gtest.h>
#include "app/api/session.hpp"
#include "app/common/speed_limiter.hpp"

using namespace app;

TEST(NetSessionTest, SetMultiThread) {
    NetSession session;
    // Testing default constructor values
    session.set_multi_thread(false, 8);
}

TEST(NetSessionTest, SetMultiThreadClamp) {
    NetSession session;
    session.set_multi_thread(true, 99);
    session.set_multi_thread(true, 0);
}

TEST(NetSessionTest, SetSpeedLimiter) {
    NetSession session;
    auto limiter = std::make_shared<SpeedLimiter>(100);
    session.set_speed_limiter(limiter, false);

    auto limiter2 = std::make_shared<SpeedLimiter>(200);
    session.set_speed_limiter(limiter2, true);
}

TEST(NetSessionTest, SetProgressCallback) {
    NetSession session;
    bool called = false;
    session.set_progress_callback([&called](int64_t, int64_t) { called = true; });
}

TEST(NetSessionTest, SetProxy) {
    NetSession session;
    session.set_proxy("http://127.0.0.1:8080");
    session.set_proxy("");  // Clear
}

TEST(NetSessionTest, SetProxyAuth) {
    NetSession session;
    session.set_proxy_auth("http", "proxy.example.com", 3128, "user", "pass");
}

TEST(NetSessionTest, AuthorizationEmptyByDefault) {
    NetSession session;
    EXPECT_TRUE(session.authorization().empty());
}

TEST(NetSessionTest, SafeJsonValid) {
    cpr::Response resp;
    resp.status_code = 200;
    resp.text = R"({"code": 200, "message": "ok"})";

    auto [parsed, error] = NetSession::safe_json(resp);
    EXPECT_EQ(parsed["code"].get<int>(), 200);
    EXPECT_FALSE(error.has_value());
}

TEST(NetSessionTest, SafeJsonInvalid) {
    cpr::Response resp;
    resp.status_code = 500;
    resp.text = "<html>error</html>";

    auto [parsed, error] = NetSession::safe_json(resp);
    EXPECT_TRUE(parsed.empty());
    EXPECT_TRUE(error.has_value());
    EXPECT_EQ(error->code, -1);
}
