@echo off
setlocal

set "MINGW_BIN=D:\Env\scoop\global\apps\mingw\current\bin"
set "PATH=%MINGW_BIN%;%PATH%"

if exist "%~dp0build-mingw" rd /s /q "%~dp0build-mingw"
cmake -B "%~dp0build-mingw" -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release "%~dp0" && cmake --build "%~dp0build-mingw"
