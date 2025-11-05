cmake_minimum_required(VERSION 3.15)
project(audacity_remote_whisper LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)

# ====================================================================
# Whisper Helper Executable (for Nyquist plugin)
# ====================================================================

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

# Installation rules
install(TARGETS whisper-helper
    RUNTIME DESTINATION "share/audacity/plug-ins"
)
