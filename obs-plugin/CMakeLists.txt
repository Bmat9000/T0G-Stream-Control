cmake_minimum_required(VERSION 3.28)
project(t0g-stream-control VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(libobs REQUIRED)
find_package(obs-frontend-api REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)

add_library(t0g-stream-control MODULE
  src/plugin-main.cpp
  src/t0g-stream-dock.cpp
  src/t0g-stream-dock.hpp
  src/t0g-settings.cpp
  src/t0g-settings.hpp
  src/tiktok-service.cpp
  src/tiktok-service.hpp
  src/tiktok-output.cpp
  src/tiktok-output.hpp
)

target_link_libraries(t0g-stream-control PRIVATE
  OBS::libobs
  OBS::obs-frontend-api
  Qt6::Core
  Qt6::Widgets
  Qt6::Network
)

set_target_properties(t0g-stream-control PROPERTIES PREFIX "")

install(TARGETS t0g-stream-control
  RUNTIME DESTINATION "obs-plugins/64bit"
  LIBRARY DESTINATION "obs-plugins/64bit")
