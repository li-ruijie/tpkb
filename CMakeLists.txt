cmake_minimum_required(VERSION 3.20)
project(tpkb C)

set(CMAKE_C_STANDARD 17)

add_executable(tpkb WIN32
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

    src/ipc.c
    src/dialog.c
    src/settings.c
    src/util.c
    res/tpkb.rc
)

target_include_directories(tpkb PRIVATE src)
target_link_libraries(tpkb PRIVATE user32 kernel32 shell32 comctl32 psapi advapi32 synchronization)

target_compile_definitions(tpkb PRIVATE UNICODE _UNICODE)

if(MSVC)
    target_compile_definitions(tpkb PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(tpkb PRIVATE /W4)
    set_property(TARGET tpkb PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    target_link_options(tpkb PRIVATE /MANIFEST:NO)
elseif(MINGW)
    target_compile_options(tpkb PRIVATE -Wall)
    target_link_options(tpkb PRIVATE -static -municode)
endif()
