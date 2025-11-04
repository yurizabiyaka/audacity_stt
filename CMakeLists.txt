cmake_minimum_required(VERSION 3.15)
project(audacity_remote_whisper LANGUAGES CXX)

cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages (vcpkg will provide imported targets)
find_package(wxWidgets REQUIRED COMPONENTS core base net)
find_package(CURL REQUIRED)

# Create the module library
add_library(mod-remote-whisper MODULE
    src/RemoteWhisperModule.cpp
    src/RemoteWhisperCommand.cpp
    src/RemoteWhisperClient.cpp
    src/RemoteWhisperJson.cpp
    src/RemoteWhisperSettings.cpp
)

# Set include directories
target_include_directories(mod-remote-whisper
    PRIVATE
        include
        ${wxWidgets_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(mod-remote-whisper
    PRIVATE
        ${wxWidgets_LIBRARIES}
        CURL::libcurl
)

# Add wxWidgets definitions and flags
target_compile_definitions(mod-remote-whisper 
    PRIVATE 
        ${wxWidgets_DEFINITIONS}
        WXINTL_NO_GETTEXT_MACRO
)

target_compile_options(mod-remote-whisper
    PRIVATE
        ${wxWidgets_CXX_FLAGS}
)

# Installation rules
install(TARGETS mod-remote-whisper
    LIBRARY DESTINATION "share/audacity/modules"
    RUNTIME DESTINATION "share/audacity/modules"
)

# ====================================================================
# Standalone Helper Executable (for Nyquist plugin)
# ====================================================================

# Create the standalone helper executable (no wxWidgets dependency)
add_executable(whisper-helper
    src/whisper-helper.cpp
)

# Link only CURL for the helper
target_link_libraries(whisper-helper
    PRIVATE
        CURL::libcurl
)

# Install helper executable to plugins folder
install(TARGETS whisper-helper
    RUNTIME DESTINATION "share/audacity/plug-ins"
)
