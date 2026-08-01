@echo off
setlocal
if not exist "%~dp0build\tatarus_llm.exe" call "%~dp0build.bat"
if errorlevel 1 exit /b %errorlevel%

set "TATARUS_LOAD="
if exist "%~dp0state\web_subject_01\host_state.json" set "TATARUS_LOAD=--load"

start "" /b powershell -NoProfile -WindowStyle Hidden -Command "Start-Sleep -Milliseconds 900; Start-Process 'http://127.0.0.1:12401/'"
"%~dp0build\tatarus_llm.exe" ^
  --provider lmstudio ^
  --memory-owner tatarus ^
  --episodic-memory anchored ^
  --config "%~dp0config\tatarus_llm.example.json" ^
  --snapshot-dir "%~dp0state\web_subject_01" ^
  --demo-port 12401 ^
  %TATARUS_LOAD%
