@echo off
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64
if errorlevel 1 exit /b %errorlevel%
set "TATARUS_CMAKE=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "TATARUS_CTEST=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe"
set "TATARUS_LLM_ROOT=%~dp0"
set "TATARUS_LLM_ROOT=%TATARUS_LLM_ROOT:~0,-1%"
"%TATARUS_CMAKE%" -S "%TATARUS_LLM_ROOT%" -B "%TATARUS_LLM_ROOT%\build" -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 exit /b %errorlevel%
"%TATARUS_CMAKE%" --build "%TATARUS_LLM_ROOT%\build"
if errorlevel 1 exit /b %errorlevel%
"%TATARUS_CTEST%" --test-dir "%TATARUS_LLM_ROOT%\build" --output-on-failure
