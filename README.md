# 123pan-CPP

突破限制 · 高效下载 · 简单易用

## 介绍

123pan 是一款第三方 123 云盘客户端，使用 C++20 和 Qt6 重写。解决了 123 云盘官方客户端的若干问题，并使用多种方式解除流量限制。

本项目是原 [Python 项目](https://github.com/123panNextGen/123pan) 的 C++ 完整移植版。

## 构建要求

- CMake >= 3.20
- C++20 编译器 (GCC 13+, Clang 16+, MSVC 2022+)
- Qt6 (Core, Gui, Widgets)
- [nlohmann/json](https://github.com/nlohmann/json) >= 3.11
- [spdlog](https://github.com/gabime/spdlog) >= 1.12
- [cpr](https://github.com/libcpr/cpr) >= 1.10
- OpenSSL >= 1.1

## 构建步骤

```shell
# 克隆仓库
git clone https://github.com/123panNextGen/123pan-cpp.git
cd 123pan-cpp

# 配置
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 编译
cmake --build build -j$(nproc)

# 运行
./build/123pan
```

### 安装依赖 (Ubuntu/Debian)

```shell
sudo apt install build-essential cmake \
    qt6-base-dev libqt6widgets6 \
    nlohmann-json3-dev libspdlog-dev \
    libcpr-dev libssl-dev
```

### 安装依赖 (Fedora)

```shell
sudo dnf install gcc-c++ cmake \
    qt6-qtbase-devel \
    nlohmann-json-devel spdlog-devel \
    cpr-devel openssl-devel
```

## 测试

```shell
# 构建并运行测试
cmake -B build -DBUILD_TESTS=ON
cmake --build build -j$(nproc)
cd build && ctest --output-on-failure
```

## 项目结构

```
src/
├── main.cpp                    # 程序入口
├── app/
│   ├── api/
│   │   ├── model.hpp/cpp       # 数据模型 (FileItemModel, ApiReturnModel等)
│   │   └── session.hpp/cpp     # NetSession - HTTP 会话管理
│   ├── common/
│   │   ├── api.hpp/cpp         # Pan123 客户端类 + 工具函数
│   │   ├── config.hpp/cpp      # ConfigManager - JSON 配置管理
│   │   ├── const.hpp           # 常量定义
│   │   ├── credential.hpp/cpp  # AES-GCM 凭据加密
│   │   ├── log.hpp/cpp         # 日志系统 (基于 spdlog)
│   │   └── speed_limiter.hpp/cpp # 令牌桶速度限制器
│   ├── data/
│   │   └── devices.hpp/cpp     # 设备指纹数据
│   ├── service/
│   │   ├── auth_service.hpp/cpp    # 认证服务
│   │   ├── download_service.hpp/cpp # 下载服务
│   │   ├── file_service.hpp/cpp    # 文件操作服务
│   │   └── upload_service.hpp/cpp  # 上传服务
│   ├── tasks/
│   │   ├── file_tasks.hpp/cpp  # 异步任务 (QRunnable)
│   │   └── signals.hpp/cpp     # Qt 信号
│   └── view/
│       ├── cloud_interface.hpp/cpp   # 云盘信息页面
│       ├── dialogs.hpp/cpp           # 通用对话框
│       ├── file_interface.hpp/cpp    # 文件浏览页面
│       ├── login_window.hpp/cpp      # 登录对话框
│       ├── main_window.hpp/cpp       # 主窗口
│       ├── setting_interface.hpp/cpp # 设置页面
│       └── transfer_interface.hpp/cpp # 传输管理页面
└── resource/
    ├── resource.qrc             # Qt 资源文件
    └── qss/                     # 样式表 (dark/light)
```

## 技术说明

- 配置和日志存储于 `~/.config/123pan/` (Linux) 或 `%APPDATA%/123pan/` (Windows)
- 密码使用 AES-256-GCM 加密存储，密钥基于机器标识派生
- 使用多线程分片下载，默认 4 线程
- 支持令牌桶算法限速（下载/上传）
- 支持 HTTP/SOCKS5 代理
- 支持暗色/亮色主题

## 许可证

Apache 2.0
