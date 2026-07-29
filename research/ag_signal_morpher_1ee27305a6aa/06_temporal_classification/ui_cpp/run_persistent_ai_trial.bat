@echo off
setlocal
set "SCRIPT_DIR=%~dp0"
set "EXE=%SCRIPT_DIR%build_stage19\AGPersistentAITrial.exe"
if not exist "%EXE%" (
  call "%SCRIPT_DIR%build_persistent_ai_trial.bat" build_stage19
  if errorlevel 1 exit /b 1
)
pushd "%SCRIPT_DIR%"
"%EXE%" stage19_confirmation --confirm
set "RESULT=%ERRORLEVEL%"
popd
exit /b %RESULT%
