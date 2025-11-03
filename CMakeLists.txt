cmake_minimum_required(VERSION 3.15)
project(audacity_remote_whisper LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(CURL REQUIRED)
find_package(wxWidgets REQUIRED COMPONENTS core base net)

if(wxWidgets_FOUND)
    include(${wxWidgets_USE_FILE})
endif()

set(AUDACITY_SOURCE_DIR "" CACHE PATH "Path to the Audacity source tree")

if(NOT AUDACITY_SOURCE_DIR)
    message(FATAL_ERROR "AUDACITY_SOURCE_DIR must be set to the root of the Audacity source tree")
endif()

add_library(mod-remote-whisper MODULE
    src/RemoteWhisperModule.cpp
    src/RemoteWhisperCommand.cpp
    src/RemoteWhisperClient.cpp
    src/RemoteWhisperJson.cpp
    src/RemoteWhisperSettings.cpp
)

target_include_directories(mod-remote-whisper
    PRIVATE
        include
        ${AUDACITY_SOURCE_DIR}/src
        ${AUDACITY_SOURCE_DIR}/src/modules
)

if(TARGET CURL::libcurl)
    target_link_libraries(mod-remote-whisper PRIVATE CURL::libcurl)
else()
    target_link_libraries(mod-remote-whisper PRIVATE ${CURL_LIBRARIES})
endif()

if(TARGET wxWidgets::wxWidgets)
    target_link_libraries(mod-remote-whisper PRIVATE wxWidgets::wxWidgets)
else()
    target_link_libraries(mod-remote-whisper PRIVATE ${wxWidgets_LIBRARIES})
endif()

install(TARGETS mod-remote-whisper
    LIBRARY DESTINATION "share/audacity/modules"
    RUNTIME DESTINATION "share/audacity/modules"
)
