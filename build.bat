@echo off
setlocal

call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\Tools\VsDevCmd.bat" -no_logo >nul 2>&1

cmake -B "%~dp0build" -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release "%~dp0" && cmake --build "%~dp0build"
