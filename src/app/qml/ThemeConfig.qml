import QtQuick
import FluentUI

/// 全局主题配置 — 复刻旧 QSS 的明快风格
/// 在 App.qml 之前加载，设置 FluentUI 主题参数
QtObject {
    id: themeConfig

    /// 应用主题
    function apply() {
        // 强调色：旧 QSS 的蓝色 #0078d4
        FluTheme.primaryColor = "#0078d4"

        // 启用原生字体渲染（Segoe UI / Microsoft YaHei）
        FluTheme.nativeText = true

        // 启用动画（FluentUI 平滑过渡）
        FluTheme.animationEnabled = true

        // 亮色模式下的微调
        // FluentUI 内部已很好地处理了暗色/亮色切换
    }

    Component.onCompleted: apply()
}
