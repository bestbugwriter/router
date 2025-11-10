cmake_minimum_required(VERSION 3.16)
project(LogPipeline VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(Threads REQUIRED)

# External dependencies
include(FetchContent)

# Asio (standalone)
FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG        asio-1-28-2
)
FetchContent_MakeAvailable(asio)
add_library(asio INTERFACE)
target_include_directories(asio INTERFACE ${asio_SOURCE_DIR}/include)
add_library(asio::asio ALIAS asio)

# nlohmann/json
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.2
)
FetchContent_MakeAvailable(nlohmann_json)

# spdlog
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.12.0
)
FetchContent_MakeAvailable(spdlog)

# inih
FetchContent_Declare(
    inih
    GIT_REPOSITORY https://github.com/benhoyt/inih.git
    GIT_TAG        r57
)
FetchContent_MakeAvailable(inih)

# Check for librdkafka
find_package(PkgConfig REQUIRED)
pkg_check_modules(RDKAFKA REQUIRED rdkafka++)

# Check for zookeeper
find_path(ZOOKEEPER_INCLUDE_DIR zookeeper/zookeeper.h)
find_library(ZOOKEEPER_LIBRARY zookeeper_mt)

if(NOT ZOOKEEPER_INCLUDE_DIR OR NOT ZOOKEEPER_LIBRARY)
    message(FATAL_ERROR "ZooKeeper library not found. Please install libzookeeper-dev")
endif()

# Check for prometheus-cpp (optional, for Forwarder)
find_package(PkgConfig QUIET)
pkg_check_modules(PROMETHEUS_CPP prometheus-cpp)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${ZOOKEEPER_INCLUDE_DIR})
include_directories(${asio_SOURCE_DIR}/asio/include)
include_directories(${inih_SOURCE_DIR})

# Compiler-specific options
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2")
endif()

# Source files
file(GLOB_RECURSE AGENT_SOURCES "src/agent/*.cpp")
file(GLOB_RECURSE FORWARDER_SOURCES "src/forwarder/*.cpp")
file(GLOB_RECURSE COMMON_SOURCES "src/common/*.cpp")

# Agent executable
add_executable(log-agent
    ${AGENT_SOURCES}
    ${COMMON_SOURCES}
)

target_link_libraries(log-agent
    PRIVATE
    asio::asio
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    inih
    ${RDKAFKA_LIBRARIES}
    ${ZOOKEEPER_LIBRARY}
    Threads::Threads
)

target_include_directories(log-agent PRIVATE ${RDKAFKA_INCLUDE_DIRS})
target_compile_options(log-agent PRIVATE ${RDKAFKA_CFLAGS_OTHER})

# Forwarder executable
add_executable(log-forwarder
    ${FORWARDER_SOURCES}
    ${COMMON_SOURCES}
)

target_link_libraries(log-forwarder
    PRIVATE
    asio::asio
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    inih
    ${RDKAFKA_LIBRARIES}
    Threads::Threads
)

target_include_directories(log-forwarder PRIVATE ${RDKAFKA_INCLUDE_DIRS})
target_compile_options(log-forwarder PRIVATE ${RDKAFKA_CFLAGS_OTHER})

# Add prometheus-cpp if available
if(PROMETHEUS_CPP_FOUND)
    target_link_libraries(log-forwarder PRIVATE ${PROMETHEUS_CPP_LIBRARIES})
    target_include_directories(log-forwarder PRIVATE ${PROMETHEUS_CPP_INCLUDE_DIRS})
    target_compile_options(log-forwarder PRIVATE ${PROMETHEUS_CPP_CFLAGS_OTHER})
    target_compile_definitions(log-forwarder PRIVATE HAVE_PROMETHEUS_CPP)
endif()

# Installation
install(TARGETS log-agent log-forwarder
    RUNTIME DESTINATION bin
)

# Configuration files
install(FILES config/agent.properties config/forwarder.properties
    DESTINATION etc/log-pipeline
)
