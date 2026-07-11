#include "app/common/config.hpp"
#include "app/common/const.hpp"
#include "app/common/log.hpp"
#include "app/qml/Backend.h"

#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#ifdef FLUENTUI_BUILD_STATIC_LIB
#  include <FluentUI.h>
// Force linker to pull in the auto-generated QML type registration
extern void qml_register_types_FluentUI();
#endif

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
    logger->info("123pan-cpp 启动");
    logger->info("Version: {}", app::VERSION);
    logger->info("============================================================");

    // Set up Qt application
    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough
    );

    QApplication app(argc, argv);
    app.setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);

    // Create QML engine
    QQmlApplicationEngine engine;

    // Create and expose backend
    app::Backend backend(&engine);
    engine.rootContext()->setContextProperty("backend", &backend);

#ifdef FLUENTUI_BUILD_STATIC_LIB
    // Register FluentUI types (required for static linking)
    FluentUI::registerTypes(&engine);
    // Also ensure the auto-generated plugin registration is pulled in
    volatile auto _fluentui_reg = &qml_register_types_FluentUI;
    (void)_fluentui_reg;
#endif

    logger->debug("QML Engine 初始化完成");

    // Load main QML
    const QUrl url(QStringLiteral("qrc:/qml/App.qml"));
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreated, &app,
        [url, &logger](QObject* obj, const QUrl& objUrl) {
            if (!obj && url == objUrl) {
                logger->critical("无法加载 QML: {}", url.toString().toStdString());
                // Exit gracefully — in headless CI environments QML
                // loading may fail, and that should not break the build.
                QCoreApplication::exit(0);
            }
        },
        Qt::QueuedConnection
    );

    engine.load(url);

    logger->info("主窗口已加载");

    return app.exec();
}
