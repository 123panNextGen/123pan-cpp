#include "app/view/main_window.hpp"
#include "app/common/config.hpp"
#include "app/common/log.hpp"

#include <QApplication>
#include <QStyleFactory>

int main(int argc, char* argv[]) {
    // Initialize logging first
    std::string config_dir = app::get_config_dir().string();
    app::init_logging(config_dir);

    auto logger = app::get_logger("123pan");

    // Load log level from config
    std::string log_level = app::ConfigManager::get_setting<std::string>("logLevel", "DEBUG");
    app::set_log_level(log_level);
    logger->info("日志等级: {}", log_level);

    logger->info("============================================================");
    logger->info("123pan 启动");
    logger->info("Version: {}", app::VERSION);
    logger->info("============================================================");

    // Set up Qt application
    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough
    );

    QApplication app(argc, argv);
    app.setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);

    // Apply Fusion style for a modern look
    app.setStyle(QStyleFactory::create("Fusion"));

    logger->debug("QApplication 初始化完成");

    // Create and show main window
    app::MainWindow window;
    window.show();

    logger->info("主窗口已显示");

    return app.exec();
}
