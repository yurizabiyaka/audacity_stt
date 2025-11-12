cmake_minimum_required(VERSION 3.15)
project(audacity_remote_whisper LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Force static linking on Windows
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options(/MT$<$<CONFIG:Debug>:d>)

    # Force static libraries
    set(BUILD_SHARED_LIBS OFF)
    set(CURL_STATICLIB ON)
endif()

# ====================================================================
# Whisper Helper Executable (for Nyquist plugin)
# ====================================================================

# Find required packages
find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)

# Create the standalone helper executable
add_executable(whisper-helper
    src/whisper-helper.cpp
    )
    
    # Link libraries
    target_link_libraries(whisper-helper
        PRIVATE
            CURL::libcurl
            nlohmann_json::nlohmann_json
)

# Define CURL_STATICLIB for static linking
if(MSVC)
target_compile_definitions(whisper-helper PRIVATE CURL_STATICLIB)
endif()

# ====================================================================
# POC Pipe Server Module DLL
# ====================================================================
find_package(wxWidgets REQUIRED)

add_library(pipeserver-mod MODULE
    POC/POC-pipeserver-mod-dll.cpp
)

target_include_directories(pipeserver-mod
    PRIVATE
        include
            ${wxWidgets_INCLUDE_DIRS}
)

target_link_libraries(pipeserver-mod
    PRIVATE
        ${wxWidgets_LIBRARIES}
)

target_compile_definitions(pipeserver-mod
    PRIVATE
        ${wxWidgets_DEFINITIONS}
)

# Set DLL properties
set_target_properties(pipeserver-mod PROPERTIES
    PREFIX ""
    OUTPUT_NAME "pipeserver-mod"
)
if(MSVC)
    target_compile_options(pipeserver-mod PRIVATE /MT$<$<CONFIG:Debug>:d>)
endif()