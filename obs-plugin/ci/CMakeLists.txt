cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version} LANGUAGES CXX)

include(compilerconfig)
include(defaults)
include(helpers)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
find_package(obs-frontend-api REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
  OBS::libobs
  OBS::obs-frontend-api
  Qt6::Core
  Qt6::Widgets
  Qt6::Network
  $<$<PLATFORM_ID:Windows>:advapi32>
)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
  src/plugin-main.cpp
  src/twitch-service.cpp
  src/twitch-service.hpp
  src/preflight-check.cpp
  src/preflight-check.hpp
  src/stream-preset.cpp
  src/stream-preset.hpp
  src/session-state.cpp
  src/session-state.hpp
  src/t0g-stream-dock.cpp
  src/t0g-stream-dock.hpp
  src/t0g-settings.cpp
  src/t0g-settings.hpp
  src/aitum-vertical.cpp
  src/aitum-vertical.hpp
  src/manual-rtmp-output.cpp
  src/manual-rtmp-output.hpp
  src/credential-store.cpp
  src/credential-store.hpp
  src/tiktok-service.cpp
  src/tiktok-service.hpp
  src/tiktok-output.cpp
  src/tiktok-output.hpp
  src/chat-message.hpp
  src/chat-feed.hpp
  src/chat-merger-dock.cpp
  src/chat-merger-dock.hpp
  src/twitch-chat-service.cpp
  src/twitch-chat-service.hpp
)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
