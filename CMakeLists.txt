cmake_minimum_required(VERSION 3.20)
project(W10Wheel C)

set(CMAKE_C_STANDARD 17)

add_executable(W10Wheel WIN32
    src/main.c
    src/config.c
    src/scroll.c
    src/event.c
    src/waiter.c
    src/kevent.c
    src/dispatch.c
    src/hook.c
    src/tray.c
    src/rawinput.c
    src/cursor.c
    src/locale.c
    src/ipc.c
    src/dialog.c
    src/util.c
    res/w10wheel.rc
)

target_include_directories(W10Wheel PRIVATE src)
target_link_libraries(W10Wheel PRIVATE user32 kernel32 shell32 comctl32 psapi)

if(MSVC)
    target_compile_definitions(W10Wheel PRIVATE UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(W10Wheel PRIVATE /W4)
    set_property(TARGET W10Wheel PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(MINGW)
    target_compile_definitions(W10Wheel PRIVATE UNICODE _UNICODE)
    target_compile_options(W10Wheel PRIVATE -Wall)
    target_link_options(W10Wheel PRIVATE -static -municode)
endif()
