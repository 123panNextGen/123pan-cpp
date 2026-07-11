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
option(BUILD_FLUENTUI_GUI "Build with FluentUI QML interface" ON)

# ============================================================
# Release optimization — minimize binary size
# ============================================================
if(CMAKE_BUILD_TYPE MATCHES "Release" OR CMAKE_BUILD_TYPE MATCHES "MinSizeRel")
    # Link-time optimization (implies -flto)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
    # Function-level sections for linker garbage collection
    add_compile_options(-ffunction-sections -fdata-sections)
    if(NOT APPLE)
        add_link_options(-Wl,--gc-sections)
    endif()
endif()

# ============================================================
# Find packages
# ============================================================
if(BUILD_GUI)
    if(BUILD_FLUENTUI_GUI)
        find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Quick Qml QuickControls2)
    else()
        find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
    endif()
endif()

# JSON library
find_package(nlohmann_json REQUIRED)

# Logging library
find_package(spdlog REQUIRED)

# spdlog compiled-lib detection:
# When spdlog is built as a compiled (shared/static) library rather than
# header-only, SPDLOG_COMPILED_LIB must be defined to avoid dllimport
# mismatches, especially on Windows with vcpkg.
get_target_property(_spdlog_type spdlog::spdlog TYPE)
if(_spdlog_type STREQUAL "SHARED_LIBRARY" OR _spdlog_type STREQUAL "STATIC_LIBRARY")
    add_compile_definitions(SPDLOG_COMPILED_LIB)
endif()
unset(_spdlog_type)

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

# -- QML UI sources (FluentUI) --
if(BUILD_FLUENTUI_GUI)
    set(FLUENTUI_QML_SOURCES
        src/app/qml/App.qml
        src/app/qml/LoginPage.qml
        src/app/qml/FilePage.qml
        src/app/qml/TransferPage.qml
        src/app/qml/CloudPage.qml
        src/app/qml/SettingsPage.qml
    )
    set(FLUENTUI_CPP_SOURCES
        src/app/qml/Backend.cpp
    )
    set(FLUENTUI_QRC
        src/resource/qml.qrc
    )

    # Build FluentUI as static library
    set(FLUENTUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(FLUENTUI_BUILD_STATIC_LIB ON CACHE BOOL "" FORCE)
    add_subdirectory(thirdparty/FluentUI)
endif()

# ============================================================
# Main executable
# ============================================================
if(BUILD_GUI)
    if(BUILD_FLUENTUI_GUI)
        qt_add_executable(123pan
            src/main.cpp
            ${COMMON_SOURCES}
            ${DATA_SOURCES}
            ${API_SOURCES}
            ${SERVICE_SOURCES}
            ${TASKS_SOURCES}
            ${FLUENTUI_CPP_SOURCES}
            ${RESOURCE_FILES}
            ${FLUENTUI_QRC}
            ${FLUENTUI_QML_SOURCES}
        )
    else()
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
    endif()

    target_include_directories(123pan PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )

    if(BUILD_FLUENTUI_GUI)
        target_link_libraries(123pan PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Widgets
            Qt6::Quick
            Qt6::Qml
            Qt6::QuickControls2
            fluentui
            fluentuiplugin
            nlohmann_json::nlohmann_json
            spdlog::spdlog
            cpr::cpr
            OpenSSL::SSL
            OpenSSL::Crypto
        )
        # Required for FluentUI static linking — enables FluentUI::registerTypes()
        target_compile_definitions(123pan PRIVATE FLUENTUI_BUILD_STATIC_LIB)
        target_include_directories(123pan PRIVATE
            ${CMAKE_SOURCE_DIR}/thirdparty/FluentUI/src
        )

    # Strip binary in Release builds
    if(CMAKE_BUILD_TYPE MATCHES "Release" OR CMAKE_BUILD_TYPE MATCHES "MinSizeRel")
        add_custom_command(TARGET 123pan POST_BUILD
            COMMAND ${CMAKE_STRIP} "$<TARGET_FILE:123pan>"
            COMMENT "Stripping 123pan binary"
        )
    endif()
    else()
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
    endif()

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
