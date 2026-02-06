cmake_minimum_required(VERSION 3.21)
project(L1GHTER LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(L1GHTER
    src/main.cpp
    src/app.cpp
    src/app.h
    src/logging.cpp
    src/logging.h
    src/config.cpp
    src/config.h
    src/process_watcher.cpp
    src/process_watcher.h
    src/desktop_duplicator.cpp
    src/desktop_duplicator.h
    src/encoder.cpp
    src/encoder.h
    src/audio_capture.cpp
    src/audio_capture.h
    src/overlay_window.cpp
    src/overlay_window.h
    src/tray_icon.cpp
    src/tray_icon.h
    src/hotkey.cpp
    src/hotkey.h
    src/ring_buffer.h
    src/util.h
)

target_compile_definitions(L1GHTER PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)

target_include_directories(L1GHTER PRIVATE src)

target_link_libraries(L1GHTER
    d3d11
    dxgi
    d3dcompiler
    d2d1
    dwrite
    dwmapi
    shlwapi
    ole32
    oleaut32
    mfplat
    mf
    mfuuid
    mfreadwrite
    avrt
    strmiids
    winmm
    comctl32
)

set_property(TARGET L1GHTER PROPERTY WIN32_EXECUTABLE TRUE)
