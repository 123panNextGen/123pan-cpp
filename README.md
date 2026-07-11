# 123pan-CPP

突破限制 · 高效下载 · 简单易用

## 介绍

123pan 是一款第三方 123 云盘客户端，使用 C++20 和 Qt6 重写。解决了 123 云盘官方客户端的若干问题，并使用多种方式解除流量限制。

本项目是原 [Python 项目](https://github.com/123panNextGen/123pan) 的 C++ 完整移植版。

UI 基于 [FluentUI](https://github.com/zhuzichu520/FluentUI)（QML Fluent Design 组件库），提供现代、美观的 Windows 11 风格界面。


## 构建要求

- CMake >= 3.20
- C++20 编译器 (GCC 13+, Clang 16+, MSVC 2022+)
- Qt6 (Core, Gui, Widgets, Quick, Qml, QuickControls2)
- [nlohmann/json](https://github.com/nlohmann/json) >= 3.11
- [spdlog](https://github.com/gabime/spdlog) >= 1.12
- [cpr](https://github.com/libcpr/cpr) >= 1.10
- OpenSSL >= 1.1

> **注意：** FluentUI 已作为 Git 子模块包含在 `thirdparty/FluentUI/` 中，首次克隆后需初始化子模块。

## 构建步骤

```shell
# 克隆仓库（含子模块）
git clone --recurse-submodules https://github.com/123panNextGen/123pan-cpp.git
cd 123pan-cpp

# 如果已克隆但未初始化子模块：
git submodule update --init --recursive

# 配置&编译
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF
cmake --build build -j$(nproc)

# 运行
./build/123pan
```

### CMake 选项

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `BUILD_GUI` | `ON` | 构建 GUI 应用 |
| `BUILD_FLUENTUI_GUI` | `ON` | 使用 FluentUI QML 界面（需 `BUILD_GUI=ON`） |
| `BUILD_TESTS` | `ON` | 构建单元测试和集成测试 |

```shell
# 传统 QWidget 模式（不含 FluentUI）
cmake -B build -DBUILD_FLUENTUI_GUI=OFF

# 仅构建测试（无 GUI）
cmake -B build -DBUILD_GUI=OFF -DBUILD_TESTS=ON

# 带 ASan/UBSan 的调试构建
cmake -B build --preset debug-sanitizers
```

### 安装依赖 (Ubuntu/Debian)

```shell
sudo apt install build-essential cmake \
    qt6-base-dev qt6-declarative-dev qt6-quickcontrols2-dev \
    nlohmann-json3-dev libspdlog-dev libcpr-dev libssl-dev
```

### 安装依赖 (Fedora)

```shell
sudo dnf install gcc-c++ cmake \
    qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtquickcontrols2-devel \
    nlohmann-json-devel spdlog-devel cpr-devel openssl-devel
```

### 安装依赖 (Arch Linux)

```shell
sudo pacman -S base-devel cmake qt6-base qt6-declarative \
    nlohmann-json spdlog cpr openssl
```

> [!TIP]
> 如果 cpr 不在系统软件源中，可手动编译安装：
> ```shell
> git clone https://github.com/libcpr/cpr.git
> cd cpr
> cmake -B build -DCPR_USE_SYSTEM_CURL=ON -DCPR_BUILD_TESTS=OFF
> cmake --build build -j$(nproc)
> sudo cmake --install build
> sudo ldconfig
> ```

## 项目结构

```
123pan-cpp/
├── CMakeLists.txt              # 顶层 CMake 配置
├── CMakePresets.json           # CMake 预设（debug/release/ci）
├── README.md
├── src/
│   ├── main.cpp                # 应用入口
│   ├── app/
│   │   ├── common/             # 公共模块
│   │   │   ├── api.cpp/hpp     # Pan123 API 门面类
│   │   │   ├── config.cpp/hpp  # 配置管理
│   │   │   ├── const.hpp       # 全局常量
│   │   │   ├── credential.cpp/hpp  # 凭据加密（AES-256-GCM）
│   │   │   ├── log.cpp/hpp     # 日志系统（spdlog）
│   │   │   └── speed_limiter.cpp/hpp # 令牌桶限速器
│   │   ├── api/
│   │   │   ├── model.cpp/hpp   # 数据模型（FileItem, UserInfo 等）
│   │   │   └── session.cpp/hpp # HTTP 会话层（cpr）
│   │   ├── data/
│   │   │   └── devices.cpp/hpp # 设备指纹伪装
│   │   ├── service/
│   │   │   ├── auth_service.cpp/hpp    # 认证服务
│   │   │   ├── file_service.cpp/hpp    # 文件操作服务
│   │   │   ├── download_service.cpp/hpp # 下载服务
│   │   │   └── upload_service.cpp/hpp  # 上传服务
│   │   ├── tasks/
│   │   │   ├── signals.cpp/hpp  # 异步信号声明
│   │   │   └── file_tasks.cpp/hpp # 异步任务（QThreadPool）
│   │   ├── view/               # QWidget 界面（BUILD_FLUENTUI_GUI=OFF 时使用）
│   │   │   ├── main_window.cpp/hpp
│   │   │   ├── login_window.cpp/hpp
│   │   │   ├── file_interface.cpp/hpp
│   │   │   ├── transfer_interface.cpp/hpp
│   │   │   ├── cloud_interface.cpp/hpp
│   │   │   ├── setting_interface.cpp/hpp
│   │   │   └── dialogs.cpp/hpp
│   │   └── qml/                # FluentUI QML 界面（默认）
│   │       ├── App.qml         # 主窗口（FluWindow + NavigationView）
│   │       ├── LoginPage.qml   # 登录页
│   │       ├── FilePage.qml    # 文件管理页
│   │       ├── TransferPage.qml # 传输管理页
│   │       ├── CloudPage.qml   # 云盘信息页
│   │       ├── SettingsPage.qml # 设置页
│   │       └── Backend.h/cpp   # C++/QML 桥接层
│   └── resource/
│       ├── resource.qrc        # 传统资源（QSS 主题）
│       ├── qml.qrc             # QML 资源
│       └── qss/                # QSS 主题文件
├── tests/
│   ├── unit/                   # 单元测试
│   │   ├── test_config.cpp
│   │   ├── test_credential.cpp
│   │   ├── test_model.cpp
│   │   ├── test_speed_limiter.cpp
│   │   └── test_api_utils.cpp
│   └── integration/
│       └── test_session.cpp    # 集成测试
└── thirdparty/
    └── FluentUI/               # FluentUI QML 组件库
```

## 测试

```shell
# 构建并运行测试
cmake -B build -DBUILD_TESTS=ON
cmake --build build -j$(nproc)
cd build && ctest --output-on-failure
```
