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
sudo apt install build-essential cmake qt6-base-dev libqt6widgets6 nlohmann-json3-dev libspdlog-dev libcpr-dev libssl-dev
```

### 安装依赖 (Fedora)

```shell
sudo dnf install gcc-c++ cmake qt6-qtbase-devel nlohmann-json-devel spdlog-devel cpr-devel openssl-devel
```

>[!TIPS]
>cpr有可能在软件原中没有，可以手动编译
>```shell
>git clone https://github.com/libcpr/cpr.git
>cd cpr
>
>cmake -B build \
>    -DCPR_USE_SYSTEM_CURL=ON \
>    -DCPR_BUILD_TESTS=OFF
>
>cmake --build build -j$(nproc)
>sudo cmake --install build
>sudo ldconfig
>```

## 测试

```shell
# 构建并运行测试
cmake -B build -DBUILD_TESTS=ON
cmake --build build -j$(nproc)
cd build && ctest --output-on-failure
```
