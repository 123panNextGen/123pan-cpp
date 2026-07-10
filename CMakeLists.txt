cmake_minimum_required(VERSION 3.20)
project(123pan-cpp VERSION 0.0.1 LANGUAGES CXX)

# C++20 required
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable Qt MOC, UIC, RCC automatically
if(BUILD_GUI)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_AUTOUIC ON)
endif()

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Export compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ============================================================
# Options
# ============================================================
option(BUILD_TESTS "Build unit and integration tests" ON)
option(BUILD_GUI "Build Qt6 GUI application" ON)

# ============================================================
# Find packages
# ============================================================
if(BUILD_GUI)
    find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
endif()

# JSON library
find_package(nlohmann_json REQUIRED)

# Logging library
find_package(spdlog REQUIRED)

# HTTP client
find_package(cpr REQUIRED)

# OpenSSL for AES-GCM encryption
find_package(OpenSSL REQUIRED)

# ============================================================
# Source files organized by module
# ============================================================

# -- common module --
set(COMMON_SOURCES
    src/app/common/const.hpp
    src/app/common/log.cpp
    src/app/common/config.cpp
    src/app/common/credential.cpp
    src/app/common/speed_limiter.cpp
    src/app/common/api.cpp
)

# -- data module --
set(DATA_SOURCES
    src/app/data/devices.cpp
)

# -- api module --
set(API_SOURCES
    src/app/api/model.cpp
    src/app/api/session.cpp
)

# -- service module --
set(SERVICE_SOURCES
    src/app/service/auth_service.cpp
    src/app/service/file_service.cpp
    src/app/service/download_service.cpp
    src/app/service/upload_service.cpp
)

# -- tasks module --
set(TASKS_SOURCES
    src/app/tasks/signals.cpp
    src/app/tasks/file_tasks.cpp
)

# -- view module --
set(VIEW_SOURCES
    src/app/view/dialogs.cpp
    src/app/view/login_window.cpp
    src/app/view/cloud_interface.cpp
    src/app/view/file_interface.cpp
    src/app/view/transfer_interface.cpp
    src/app/view/setting_interface.cpp
    src/app/view/main_window.cpp
)

# -- resources --
set(RESOURCE_FILES
    src/resource/resource.qrc
)

# ============================================================
# Main executable
# ============================================================
if(BUILD_GUI)
    qt_add_executable(123pan
        src/main.cpp
        ${COMMON_SOURCES}
        ${DATA_SOURCES}
        ${API_SOURCES}
        ${SERVICE_SOURCES}
        ${TASKS_SOURCES}
        ${VIEW_SOURCES}
        ${RESOURCE_FILES}
    )

    target_include_directories(123pan PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )

    target_link_libraries(123pan PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        nlohmann_json::nlohmann_json
        spdlog::spdlog
        cpr::cpr
        OpenSSL::SSL
        OpenSSL::Crypto
    )

    # Platform-specific settings
    if(WIN32)
        set_target_properties(123pan PROPERTIES WIN32_EXECUTABLE TRUE)
    elseif(APPLE)
        set_target_properties(123pan PROPERTIES MACOSX_BUNDLE TRUE)
    endif()
endif()

# ============================================================
# Tests
# ============================================================
if(BUILD_TESTS)
    enable_testing()
    find_package(GTest REQUIRED)

    set(TEST_SOURCES
        tests/unit/test_speed_limiter.cpp
        tests/unit/test_config.cpp
        tests/unit/test_credential.cpp
        tests/unit/test_model.cpp
        tests/unit/test_api_utils.cpp
        tests/integration/test_session.cpp
    )

    # Build core library for testing (without main)
    add_library(123pan_core STATIC
        ${COMMON_SOURCES}
        ${DATA_SOURCES}
        ${API_SOURCES}
        ${SERVICE_SOURCES}
    )

    target_include_directories(123pan_core PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )

    target_link_libraries(123pan_core PUBLIC
        nlohmann_json::nlohmann_json
        spdlog::spdlog
        cpr::cpr
        OpenSSL::SSL
        OpenSSL::Crypto
    )

    foreach(test_file ${TEST_SOURCES})
        get_filename_component(test_name ${test_file} NAME_WE)
        add_executable(${test_name} ${test_file})
        target_include_directories(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src
        )
        target_link_libraries(${test_name} PRIVATE
            123pan_core
            GTest::gtest
            GTest::gtest_main
        )
        gtest_discover_tests(${test_name})
    endforeach()
endif()

# ============================================================
# Install rules
# ============================================================
if(BUILD_GUI)
    install(TARGETS 123pan DESTINATION bin)
    install(DIRECTORY src/resource/qss DESTINATION share/123pan)
endif()
