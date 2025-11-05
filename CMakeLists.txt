cmake_minimum_required(VERSION 3.15)
project(audacity_remote_whisper LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find CURL (required for HTTP requests)
find_package(CURL REQUIRED)

# ====================================================================
# Whisper Helper Executable (for Nyquist plugin)
# ====================================================================

# Create the standalone helper executable
add_executable(whisper-helper
    src/whisper-helper.cpp
)

# Link CURL library
target_link_libraries(whisper-helper
    PRIVATE
        CURL::libcurl
)

# Installation rules
install(TARGETS whisper-helper
    RUNTIME DESTINATION "share/audacity/plug-ins"
)
