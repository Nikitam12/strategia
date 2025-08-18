cmake_minimum_required(VERSION 3.20)

project(strategia LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(ENABLE_POSTGRES "Enable PostgreSQL (libpqxx) storage" OFF)

include(FetchContent)

# CPM.cmake to fetch dependencies easily
FetchContent_Declare(
  CPM
  GIT_REPOSITORY https://github.com/cpm-cmake/CPM.cmake.git
  GIT_TAG origin/master
)
FetchContent_MakeAvailable(CPM)

# nlohmann/json
CPMAddPackage(
  NAME nlohmann_json
  GITHUB_REPOSITORY nlohmann/json
  VERSION 3.11.3
)

# ixwebsocket (WebSocket client)
CPMAddPackage(
  NAME ixwebsocket
  GITHUB_REPOSITORY machinezone/IXWebSocket
  VERSION 11.4.3
  OPTIONS
    "IXWEBSOCKET_USE_TLS ON"
    "IXWEBSOCKET_INSTALL OFF"
)

# cpr (HTTP REST)
set(CPR_USE_SYSTEM_CURL OFF)
CPMAddPackage(
  NAME cpr
  GITHUB_REPOSITORY libcurl/cpr
  VERSION 1.11.0
  OPTIONS
    "CMAKE_USE_OPENSSL ON"
    "BUILD_SHARED_LIBS OFF"
    "CURL_ZLIB ON"
)

if(ENABLE_POSTGRES)
  # libpqxx needs libpq installed on the system; set PQXX_WITH_SYSTEM_LIBPQ=ON to use system libpq
  CPMAddPackage(
    NAME libpqxx
    GITHUB_REPOSITORY jtv/libpqxx
    VERSION 7.9.1
    OPTIONS
      "BUILD_SHARED_LIBS OFF"
  )
endif()

add_library(strategia_lib
  src/aggregator.cpp
  src/time_utils.hpp
  src/config.hpp
  src/exchanges/exchange_client.hpp
  src/exchanges/binance_client.cpp
  src/exchanges/binance_client.hpp
  src/exchanges/okx_client.cpp
  src/exchanges/okx_client.hpp
  src/storage/storage_writer.hpp
  src/storage/csv_writer.cpp
  src/storage/csv_writer.hpp
)

target_include_directories(strategia_lib PUBLIC src)

target_link_libraries(strategia_lib PUBLIC
  nlohmann_json::nlohmann_json
  ixwebsocket
  cpr::cpr
)

if(ENABLE_POSTGRES AND TARGET pqxx)
  target_sources(strategia_lib PRIVATE
    src/storage/postgres_writer.cpp
    src/storage/postgres_writer.hpp
  )
  target_link_libraries(strategia_lib PUBLIC pqxx)
  target_compile_definitions(strategia_lib PUBLIC STRATEGIA_ENABLE_POSTGRES)
endif()

add_executable(strategia src/main.cpp)
target_link_libraries(strategia PRIVATE strategia_lib)


