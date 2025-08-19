cmake_minimum_required(VERSION 3.20)

project(strategia LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(ENABLE_POSTGRES "Enable PostgreSQL (libpqxx) storage" OFF)
option(USE_CPM_FETCH "Use CPM.cmake to fetch dependencies from the internet" OFF)
# Optional paths for offline builds
set(CPR_SOURCE_DIR "" CACHE PATH "Path to local cpr source (optional)")
set(IXWEBSOCKET_SOURCE_DIR "" CACHE PATH "Path to local IXWebSocket source (optional)")
option(ENABLE_REST_BACKFILL "Enable REST backfill using cpr" ON)
option(ENABLE_WEBSOCKETS "Enable WebSocket streaming via IXWebSocket" OFF)
option(USE_LIBCURL_FOR_REST "Use libcurl for REST backfill instead of cpr" ON)

# Threads (pthread)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

if(USE_CPM_FETCH)
  # Bootstrap CPM only when explicitly enabled
  set(CPM_DOWNLOAD_VERSION 0.40.5)
  file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/cmake")
  if(NOT EXISTS "${CMAKE_BINARY_DIR}/cmake/CPM.cmake")
    file(DOWNLOAD
      "https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/cpm.cmake"
      "${CMAKE_BINARY_DIR}/cmake/CPM.cmake"
      TLS_VERIFY ON
    )
  endif()
  include("${CMAKE_BINARY_DIR}/cmake/CPM.cmake")
endif()

if(USE_CPM_FETCH)
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
else()
  # Use system-installed packages or local sources, no network access
  find_package(nlohmann_json 3.7 CONFIG REQUIRED)

  # REST backfill deps
  if(ENABLE_REST_BACKFILL)
    if(USE_LIBCURL_FOR_REST)
      find_package(CURL REQUIRED)
    else()
      if(CPR_SOURCE_DIR)
        message(STATUS "Using local cpr from: ${CPR_SOURCE_DIR}")
        set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
        set(CPR_USE_SYSTEM_CURL OFF CACHE BOOL "" FORCE)
        set(CURL_ZLIB ON CACHE BOOL "" FORCE)
        set(CMAKE_USE_OPENSSL ON CACHE BOOL "" FORCE)
        add_subdirectory(${CPR_SOURCE_DIR} _cpr_build)
      elseif(EXISTS ${CMAKE_SOURCE_DIR}/third_party/cpr/CMakeLists.txt)
        message(STATUS "Using vendored cpr from third_party/cpr")
        set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
        set(CPR_USE_SYSTEM_CURL OFF CACHE BOOL "" FORCE)
        set(CURL_ZLIB ON CACHE BOOL "" FORCE)
        set(CMAKE_USE_OPENSSL ON CACHE BOOL "" FORCE)
        add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/cpr _cpr_build)
      else()
        find_package(cpr CONFIG REQUIRED)
      endif()
    endif()
  endif()

  if(ENABLE_WEBSOCKETS)
    # IXWebSocket: prefer local source if provided
    if(IXWEBSOCKET_SOURCE_DIR)
      message(STATUS "Using local IXWebSocket from: ${IXWEBSOCKET_SOURCE_DIR}")
      set(IXWEBSOCKET_USE_TLS ON CACHE BOOL "" FORCE)
      set(IXWEBSOCKET_INSTALL OFF CACHE BOOL "" FORCE)
      add_subdirectory(${IXWEBSOCKET_SOURCE_DIR} _ixws_build)
    elseif(EXISTS ${CMAKE_SOURCE_DIR}/third_party/IXWebSocket/CMakeLists.txt)
      message(STATUS "Using vendored IXWebSocket from third_party/IXWebSocket")
      set(IXWEBSOCKET_USE_TLS ON CACHE BOOL "" FORCE)
      set(IXWEBSOCKET_INSTALL OFF CACHE BOOL "" FORCE)
      add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/IXWebSocket _ixws_build)
    else()
      find_package(ixwebsocket CONFIG REQUIRED)
    endif()
  endif()
endif()

if(ENABLE_POSTGRES)
  # Use system pqxx; no downloads
  find_package(pqxx CONFIG REQUIRED)
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
)

if(ENABLE_WEBSOCKETS)
  target_link_libraries(strategia_lib PUBLIC ixwebsocket)
  target_compile_definitions(strategia_lib PUBLIC STRATEGIA_ENABLE_WEBSOCKETS)
endif()

# Link pthreads for std::thread
target_link_libraries(strategia_lib PUBLIC Threads::Threads)

if(ENABLE_REST_BACKFILL)
  target_compile_definitions(strategia_lib PUBLIC STRATEGIA_ENABLE_REST_BACKFILL)
  if(USE_LIBCURL_FOR_REST)
    target_sources(strategia_lib PRIVATE
      src/http/http_client.cpp
      src/http/http_client.hpp
      src/cpr/cpr.h
    )
    target_link_libraries(strategia_lib PUBLIC CURL::libcurl)
    target_compile_definitions(strategia_lib PUBLIC STRATEGIA_USE_LIBCURL_REST)
  else()
    target_link_libraries(strategia_lib PUBLIC cpr::cpr)
  endif()
endif()

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


