@echo off
setlocal
set "APP=%~dp0build\TATARUS_ResearchUI.exe"
if not exist "%APP%" call "%~dp0build_ui.bat"
if errorlevel 1 exit /b 1
start "" "%APP%"
endlocal
