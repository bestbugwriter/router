cmake_minimum_required(VERSION 3.16)
project(LogPipeline VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 选项：是否使用静态链接
option(BUILD_STATIC "Build static binary" OFF)
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

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
add_library(inih ${inih_SOURCE_DIR}/ini.c)
target_include_directories(inih PUBLIC ${inih_SOURCE_DIR})

# 可选：zlib 支持压缩
find_package(ZLIB)
if(ZLIB_FOUND)
    add_compile_definitions(HAVE_ZLIB)
endif()

# 可选：snappy 支持压缩
FetchContent_Declare(
    snappy
    GIT_REPOSITORY https://github.com/google/snappy.git
    GIT_TAG        1.1.9
)
FetchContent_MakeAvailable(snappy)
add_compile_definitions(HAVE_SNAPPY)
# Check for librdkafka
find_package(PkgConfig REQUIRED)
pkg_check_modules(RDKAFKA REQUIRED rdkafka++)

if(RDKAFKA_FOUND)
    add_compile_definitions(HAVE_LIBRDKAFKA)
endif()

# Check for zookeeper
find_path(ZOOKEEPER_INCLUDE_DIR zookeeper/zookeeper.h)
find_library(ZOOKEEPER_LIBRARY zookeeper_mt)

if(NOT ZOOKEEPER_INCLUDE_DIR OR NOT ZOOKEEPER_LIBRARY)
    message(STATUS "ZooKeeper library not found. Building without ZooKeeper support.")
    set(ZOOKEEPER_LIBRARY "")
else()
    add_compile_definitions(HAVE_ZOOKEEPER)
endif()

# Check for prometheus-cpp (optional, for Metrics)
find_package(PkgConfig QUIET)
pkg_check_modules(PROMETHEUS_CPP prometheus-cpp)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
if(ZOOKEEPER_INCLUDE_DIR)
    include_directories(${ZOOKEEPER_INCLUDE_DIR})
endif()
include_directories(${asio_SOURCE_DIR}/asio/include)
include_directories(${inih_SOURCE_DIR})

# Compiler-specific options
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2")
    
    # 静态编译选项
    if(BUILD_STATIC)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
        message(STATUS "Building static binaries")
    endif()
endif()

# Source files
file(GLOB_RECURSE AGENT_SOURCES "src/agent/*.cpp")
file(GLOB_RECURSE ROUTER_SOURCES "src/router/*.cpp")
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
    Threads::Threads
)

if(ZOOKEEPER_LIBRARY)
    target_link_libraries(log-agent PRIVATE ${ZOOKEEPER_LIBRARY})
endif()

if(ZLIB_FOUND)
    target_link_libraries(log-agent PRIVATE ZLIB::ZLIB)
endif()

target_link_libraries(log-agent PRIVATE snappy)
target_include_directories(log-agent PRIVATE ${RDKAFKA_INCLUDE_DIRS})
target_compile_options(log-agent PRIVATE ${RDKAFKA_CFLAGS_OTHER})

# Router executable (replacing Forwarder)
add_executable(log-router
    ${ROUTER_SOURCES}
    ${COMMON_SOURCES}
)

target_link_libraries(log-router
    PRIVATE
    asio::asio
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    inih
    ${RDKAFKA_LIBRARIES}
    Threads::Threads
)

if(ZOOKEEPER_LIBRARY)
    target_link_libraries(log-router PRIVATE ${ZOOKEEPER_LIBRARY})
endif()

if(ZLIB_FOUND)
    target_link_libraries(log-router PRIVATE ZLIB::ZLIB)
endif()

target_link_libraries(log-router PRIVATE snappy)

target_include_directories(log-router PRIVATE ${RDKAFKA_INCLUDE_DIRS})
target_compile_options(log-router PRIVATE ${RDKAFKA_CFLAGS_OTHER})

# Add prometheus-cpp if available
if(PROMETHEUS_CPP_FOUND)
    target_link_libraries(log-router PRIVATE ${PROMETHEUS_CPP_LIBRARIES})
    target_include_directories(log-router PRIVATE ${PROMETHEUS_CPP_INCLUDE_DIRS})
    target_compile_options(log-router PRIVATE ${PROMETHEUS_CPP_CFLAGS_OTHER})
    target_compile_definitions(log-router PRIVATE HAVE_PROMETHEUS_CPP)
endif()

# Installation
install(TARGETS log-agent log-router
    RUNTIME DESTINATION bin
)

# Configuration files
install(FILES 
    config/agent.properties 
    config/router.properties
    DESTINATION etc/log-pipeline
)
