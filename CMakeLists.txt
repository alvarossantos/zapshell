cmake_minimum_required(VERSION 3.21)
project(zapshell VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.10 REQUIRED COMPONENTS Core Widgets WebEngineWidgets WebChannel Test)

add_library(zapshell_core STATIC
    src/cli.cpp
    src/paths.cpp
)
target_include_directories(zapshell_core PUBLIC src)
target_link_libraries(zapshell_core PUBLIC Qt6::Core)

add_executable(zapshell src/main.cpp)
target_link_libraries(zapshell PRIVATE zapshell_core Qt6::Widgets Qt6::WebEngineWidgets Qt6::WebChannel)
target_compile_definitions(zapshell PRIVATE ZAPSHELL_VERSION="${PROJECT_VERSION}")

enable_testing()
add_subdirectory(tests)
