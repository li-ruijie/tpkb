@echo off
setlocal

call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\Tools\VsDevCmd.bat" -no_logo -arch=amd64 >nul 2>&1

if exist "%~dp0build-msvc" rd /s /q "%~dp0build-msvc"
cmake -B "%~dp0build-msvc" -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release "%~dp0" && cmake --build "%~dp0build-msvc"
