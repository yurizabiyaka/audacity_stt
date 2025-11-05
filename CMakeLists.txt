cmake_minimum_required(VERSION 3.15)
project(audacity_remote_whisper LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Force static linking on Windows
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options(/MT$<$<CONFIG:Debug>:d>)

    # Force static libraries - must be set BEFORE find_package
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(CURL_STATICLIB ON CACHE BOOL "" FORCE)
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".lib" ".a")

    # Prefer static libraries
    set(CMAKE_FIND_LIBRARY_PREFIXES "lib" "")
endif()

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

# Define CURL_STATICLIB for static linking
if(MSVC)
    target_compile_definitions(whisper-helper PRIVATE CURL_STATICLIB)

    # When linking CURL statically on Windows, we need these additional libraries
    target_link_libraries(whisper-helper PRIVATE
        ws2_32
        wldap32
        advapi32
        crypt32
        normaliz
    )
endif()

# Installation rules
install(TARGETS whisper-helper
    RUNTIME DESTINATION "share/audacity/plug-ins"
)
