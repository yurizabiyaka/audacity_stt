cmake_minimum_required(VERSION 3.15)
project(audacity_remote_whisper LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Prefer module find over config for CURL
set(CMAKE_FIND_PACKAGE_PREFER_CONFIG OFF)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    add_compile_definitions(CURL_STATICLIB)
endif()

# Try FindCURL module (what vcpkg integrates with)
find_package(CURL REQUIRED MODULE)
find_package(nlohmann_json REQUIRED CONFIG)

# Debug prints so you can see what was found
message(STATUS "CURL_FOUND=${CURL_FOUND}")
message(STATUS "CURL_INCLUDE_DIRS=${CURL_INCLUDE_DIRS}")
message(STATUS "CURL_LIBRARIES=${CURL_LIBRARIES}")

add_executable(whisper-helper
    src/whisper-helper.cpp
)

target_link_libraries(whisper-helper
    PRIVATE
        CURL::libcurl
        nlohmann_json::nlohmann_json
)

install(TARGETS whisper-helper
    RUNTIME DESTINATION "share/audacity/plug-ins"
)
