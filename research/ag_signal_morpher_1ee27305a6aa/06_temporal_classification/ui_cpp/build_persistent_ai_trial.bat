@echo off
setlocal
set "SCRIPT_DIR=%~dp0"
set "BUILD_NAME=build_stage19"
if not "%~1"=="" set "BUILD_NAME=%~1"
set "BUILD_DIR=%SCRIPT_DIR%%BUILD_NAME%"
set "VSDEV=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat"
if not exist "%VSDEV%" (
  echo Visual Studio 2022 Build Tools wurden nicht gefunden.
  exit /b 1
)
call "%VSDEV%" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b 1
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
set "KERNEL_INCLUDE=%SCRIPT_DIR%..\..\..\..\exports\generated"
cl /nologo /std:c++20 /EHsc /O2 /W4 /permissive- /utf-8 ^
  /Fo"%BUILD_DIR%\\" ^
  /I"%KERNEL_INCLUDE%" ^
  "%SCRIPT_DIR%persistent_ai_trial.cpp" "%SCRIPT_DIR%cognitive_bridge.cpp" "%SCRIPT_DIR%nervous_system.cpp" ^
  /Fe:"%BUILD_DIR%\AGPersistentAITrial.exe"
if errorlevel 1 exit /b 1
echo Build erfolgreich: %BUILD_DIR%\AGPersistentAITrial.exe
endlocal
