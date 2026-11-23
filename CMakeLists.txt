cmake_minimum_required(VERSION 3.21)
project(zapshell VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

option(ZAPSHELL_BUILD_TESTS "Build the test suite" ON)

set(ZS_QT_COMPONENTS Core DBus Widgets WebEngineWidgets WebChannel)
if(ZAPSHELL_BUILD_TESTS)
    list(APPEND ZS_QT_COMPONENTS Test)
endif()
find_package(Qt6 6.10 REQUIRED COMPONENTS ${ZS_QT_COMPONENTS})

add_library(zapshell_core STATIC
    src/backup.cpp
    src/cli.cpp
    src/instance.cpp
    src/notify.cpp
    src/paths.cpp
    src/stats.cpp
    src/web.cpp
)
target_include_directories(zapshell_core PUBLIC src)
target_link_libraries(zapshell_core
    PUBLIC Qt6::Core Qt6::DBus
    PRIVATE Qt6::WebEngineCore
)

add_library(zapshell_ui STATIC
    src/app.cpp
    src/profile.cpp
    src/tray.cpp
    src/window.cpp
)
target_link_libraries(zapshell_ui
    PUBLIC zapshell_core
    PRIVATE Qt6::Widgets Qt6::WebEngineWidgets Qt6::WebEngineCore
)

add_executable(zapshell src/main.cpp)
target_link_libraries(zapshell PRIVATE zapshell_ui Qt6::Widgets)
target_compile_definitions(zapshell PRIVATE ZAPSHELL_VERSION="${PROJECT_VERSION}")

if(ZAPSHELL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
