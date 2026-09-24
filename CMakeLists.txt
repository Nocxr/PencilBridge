cmake_minimum_required(VERSION 3.24)

project(PencilBridge LANGUAGES CXX)

add_executable(PencilBridge WIN32
    src/main.cpp
    src/web_ui.h
)

target_compile_features(PencilBridge PRIVATE cxx_std_20)

target_compile_definitions(PencilBridge PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _WIN32_WINNT=0x0A00
    WINVER=0x0A00
)

target_link_libraries(PencilBridge PRIVATE
    bcrypt
    crypt32
    dwmapi
    ole32
    secur32
    user32
    uxtheme
    windowscodecs
    ws2_32
)

if(MSVC)
    target_compile_options(PencilBridge PRIVATE /W4 /permissive- /EHsc /utf-8)
endif()
