cmake_minimum_required(VERSION 3.15)
project(audacity_remote_whisper LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
)

target_link_libraries(mod-remote-whisper
    PRIVATE
        ${wxWidgets_LIBRARIES}
        ${CURL_LIBRARIES}
)

if(TARGET wxWidgets::wxWidgets)
    target_link_libraries(mod-remote-whisper PRIVATE wxWidgets::wxWidgets)
endif()

find_package(CURL CONFIG REQUIRED)
find_package(wxWidgets CONFIG REQUIRED COMPONENTS core base net)

#if(wxWidgets_FOUND)
#    include(${wxWidgets_USE_FILE})
#endif()
target_link_libraries(mod-remote-whisper PRIVATE wxWidgets::wxWidgets)

target_compile_definitions(mod-remote-whisper PRIVATE WXINTL_NO_GETTEXT_MACRO)

install(TARGETS mod-remote-whisper
    LIBRARY DESTINATION "share/audacity/modules"
    RUNTIME DESTINATION "share/audacity/modules"
)
