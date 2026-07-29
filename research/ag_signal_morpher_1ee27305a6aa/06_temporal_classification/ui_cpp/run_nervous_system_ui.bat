@echo off
setlocal
set "SCRIPT_DIR=%~dp0"
set "EXE=%SCRIPT_DIR%build_stage23\TATARUS.exe"
if not exist "%EXE%" (
  call "%SCRIPT_DIR%build_ui.bat" build_stage23
  if errorlevel 1 exit /b 1
)
pushd "%SCRIPT_DIR%build_stage23"
start "" "TATARUS.exe"
popd
endlocal
